#include "JuiceManager.h"
#include "signaling.h"

JuiceAgent::JuiceAgent(const SNetAddr& id, std::string init_message = "")
: m_p2p_state(JUICE_STATE_DISCONNECTED), m_id{id}, m_logger{ g_root_logger, "P2P Agent", id.b64() } {
	juice_config_t config{
		.stun_server_host = g_config.stun_server.c_str(),
		.stun_server_port = g_config.stun_port,

		.cb_state_changed = on_state_changed,
		.cb_candidate = on_candidate,
		.cb_gathering_done = on_gathering_done,
		.cb_recv = on_recv,
		.user_ptr = this,
	};
	m_agent = juice_create(&config);

	if (!init_message.empty()) {
		handle_signal_packet(init_message);
	}
	juice_get_local_description(m_agent, m_sdp, sizeof(m_sdp));

	g_signaling_socket.send_packet(m_id, SignalMessageType::JuiceLocalDescription, m_sdp);
	m_logger.trace("Init - local SDP {}", m_sdp);
	juice_gather_candidates(m_agent);
}
JuiceAgent::~JuiceAgent() {
	juice_destroy(m_agent);
	m_logger.debug("Agent {} closed", m_id.b64());
}

void JuiceAgent::handle_signal_packet(const SignalPacket& packet) {
	switch (packet.message_type) {
		case SignalMessageType::JuiceLocalDescription: {
			juice_set_remote_description(m_agent, packet.data.c_str());
			m_logger.trace("Received remote description:\n{}", packet.data);
		} break;
		case SignalMessageType::JuciceCandidate: {
			juice_add_remote_candidate(m_agent, packet.data.c_str());
			m_logger.trace("Received remote candidate {}", packet.data);
		} break;
		case SignalMessageType::JuiceDone: {
			juice_set_remote_gathering_done(m_agent);
			m_logger.trace("Remote gathering done");
		} break;
	}
}

void JuiceAgent::send_message(void* data, size_t size) {
	if (m_p2p_state == JUICE_STATE_CONNECTED || m_p2p_state == JUICE_STATE_COMPLETED) {
		m_logger.trace("Sending message {}", std::string{(const char*)data, size});
		juice_send(m_agent, (const char*)data, size);
	} else if (m_p2p_state == JUICE_STATE_FAILED) {
		m_logger.error("Trying to send message but P2P connection failed");
	} else {
		m_logger.error("Trying to send message but P2P connection is in unexpected state");
	}
}

void JuiceAgent::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
	JuiceAgent& parent = *(JuiceAgent*)user_ptr;
	parent.m_p2p_state = state;
	if (state == JUICE_STATE_CONNECTED) {
		parent.m_logger.info("Initially connected");
	} else if (state == JUICE_STATE_COMPLETED) {
		parent.m_logger.info("Connecttion negotiation finished");
		char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN);

		if (std::string{local}.find("typ relay") != std::string::npos) {
			parent.m_logger.warn("local connection is relayed, performance may be affected");
		}
		if (std::string{remote}.find("typ relay") != std::string::npos) {
			parent.m_logger.warn("remote connection is relayed, performance may be affected");
		}
		parent.m_logger.debug("Final candidates were local: {} remote: {}", local, remote);
	} else if (state == JUICE_STATE_FAILED) {
		parent.m_logger.error("Could not connect, gave up");
	}
}

void JuiceAgent::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr){
	auto& parent = *(JuiceAgent*)user_ptr;
	g_signaling_socket.send_packet(parent.m_id, SignalMessageType::JuciceCandidate, sdp);

}

void JuiceAgent::on_gathering_done(juice_agent_t* agent, void* user_ptr){
	auto& parent = *(JuiceAgent*)user_ptr;
	g_signaling_socket.send_packet(parent.m_id, SignalMessageType::JuiceDone);
}

void JuiceAgent::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
	JuiceAgent& parent = *(JuiceAgent*)user_ptr;
	receive_queue.emplace(GamePacket{parent.m_id, data, size});
	SetEvent(g_receive_event);
	parent.m_logger.trace("received: {}", std::string{data, size});
}

JuiceAgent* JuiceManager::maybe_get_agent(const std::string& id) {
	auto it = m_agents.find(id);
	if (it != m_agents.end()) {
		return &it->second;
	}
	return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const std::string& id) {
	if (!m_agents.contains(id)) {
		const auto [it, _] = m_agents.emplace(id, id);
		return it->second;
	}
	return m_agents.at(id);
}

JuiceManager::~JuiceManager() {
	m_logger.debug("Closing P2P Agents");
	m_agents.clear();
};

void JuiceManager::send_p2p(const std::string& id, void* data, size_t size) {
	auto& agent = ensure_agent(id);
	agent.send_message(data, size);
}

void JuiceManager::handle_signal_packet(const SignalPacket& packet) {
	auto peer_string = std::string((char*)packet.peer_id.address, sizeof(SNetAddr));
	m_logger.trace("Received message for {}: {}", peer_string, packet.data);

	auto& peer_agent = ensure_agent(peer_string);
	peer_agent.handle_signal_packet(packet);
}

void JuiceManager::send_all(void* data, const size_t size) {
	for (auto& [name, agent] : m_agents) {
		m_logger.debug("Sending message peer {} with status: {}\n", (const char*)agent.m_id.address, as_string(agent.m_p2p_state));
		agent.send_message(data, size);
	}
}

juice_state JuiceManager::peer_status(const SNetAddr& peer_id) {
	// NOTE: It's ugly but I'm still working on changing out strings for SNetAddr
	auto id = std::string{(char*)peer_id.address, sizeof(SNetAddr)};
	if (auto agent = maybe_get_agent(id)) {
		return agent->m_p2p_state;
	}
	return juice_state(JUICE_STATE_DISCONNECTED);
}
