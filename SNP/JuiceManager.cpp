#include "JuiceManager.h"
#include "Signaling.h"
#include "CrownLink.h"

JuiceAgent::JuiceAgent(const NetAddress& address, std::string init_message = "")
: m_p2p_state(JUICE_STATE_DISCONNECTED), m_address{address}, m_logger{ Logger::root(), "P2P Agent", address.b64() } {
	const auto& snp_config = SnpConfig::instance();
	juice_config_t config{
		.stun_server_host = snp_config.stun_server.c_str(),
		.stun_server_port = snp_config.stun_port,

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
	char sdp[JUICE_MAX_SDP_STRING_LEN]{};
	juice_get_local_description(m_agent, sdp, sizeof(sdp));

	g_crown_link->signaling_socket().send_packet(m_address, SignalMessageType::JuiceLocalDescription, sdp);
	m_logger.trace("Init - local SDP {}", sdp);
	juice_gather_candidates(m_agent);
}

JuiceAgent::~JuiceAgent() {
	m_logger.debug("Agent {} closed", m_address.b64());
    juice_destroy(m_agent);
}

void JuiceAgent::handle_signal_packet(const SignalPacket& packet) {
	switch (packet.message_type) {
	case SignalMessageType::JuiceLocalDescription: {
		m_logger.trace("Received remote description:\n{}", packet.data);
		juice_set_remote_description(m_agent, packet.data.c_str());
	} break;
	case SignalMessageType::JuciceCandidate: {
		m_logger.trace("Received remote candidate {}", packet.data);
		juice_add_remote_candidate(m_agent, packet.data.c_str());
	} break;
	case SignalMessageType::JuiceDone: {
		m_logger.trace("Remote gathering done");
		juice_set_remote_gathering_done(m_agent);
	} break;
	}
}

void JuiceAgent::send_message(void* data, size_t size) {
	switch (m_p2p_state) {
	case JUICE_STATE_CONNECTED:
	case JUICE_STATE_COMPLETED: {
		m_logger.trace("Sending message {}", std::string{(const char*)data, size});
		juice_send(m_agent, (const char*)data, size);
	} break;
	case JUICE_STATE_FAILED: {
		m_logger.error("Trying to send message but P2P connection failed");
	} break;
	default: {
		m_logger.error("Trying to send message but P2P connection is in unexpected state");
	} break;
	}
}

void JuiceAgent::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
	JuiceAgent& parent = *(JuiceAgent*)user_ptr;
	parent.m_p2p_state = state;
	switch (state) {
	case JUICE_STATE_CONNECTED: {
		parent.m_logger.info("Initially connected");
	} break;
	case JUICE_STATE_COMPLETED: {
		parent.m_logger.info("Connecttion negotiation finished");
		char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN);

		if (std::string{local}.find("typ relay") != std::string::npos) {
			parent.m_logger.warn("Local connection is relayed, performance may be affected");
		}
		if (std::string{remote}.find("typ relay") != std::string::npos) {
			parent.m_logger.warn("Remote connection is relayed, performance may be affected");
		}
		parent.m_logger.debug("Final candidates were local: {} remote: {}", local, remote);
	} break;
	case JUICE_STATE_FAILED: {
		parent.m_logger.error("Could not connect, gave up");
	} break;
	}
}

void JuiceAgent::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr){
	auto& parent = *(JuiceAgent*)user_ptr;
	g_crown_link->signaling_socket().send_packet(parent.m_address, SignalMessageType::JuciceCandidate, sdp);
}

void JuiceAgent::on_gathering_done(juice_agent_t* agent, void* user_ptr){
	auto& parent = *(JuiceAgent*)user_ptr;
	g_crown_link->signaling_socket().send_packet(parent.m_address, SignalMessageType::JuiceDone);
}

void JuiceAgent::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
	auto& parent = *(JuiceAgent*)user_ptr;
	g_crown_link->receive_queue().emplace(GamePacket{parent.m_address, data, size});
	SetEvent(g_receive_event);
	parent.m_logger.trace("received: {}", std::string{data, size});
}

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address) {
	auto it = m_agents.find(address);
	if (it != m_agents.end()) {
		return it->second.get();
	}
	return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address) {
	if (!m_agents.contains(address)) {
		const auto [it, _] = m_agents.emplace(address, std::make_unique<JuiceAgent>(address));
		return *it->second;
	}
	return *m_agents.at(address);
}

void JuiceManager::clear_inactive_agents() {
	std::erase_if(m_agents, [this](const auto& pair) {
		const auto& [_, agent] = pair;
		return agent->state() == JUICE_STATE_DISCONNECTED || agent->state() == JUICE_STATE_FAILED;
	});
}

void JuiceManager::send_p2p(const NetAddress& address, void* data, size_t size) {
	auto& agent = ensure_agent(address);
	agent.send_message(data, size);
}

void JuiceManager::handle_signal_packet(const SignalPacket& packet) {
	const auto& peer = packet.peer_address;
	m_logger.trace("Received message for {}: {}", peer.b64(), packet.data);

	auto& peer_agent = ensure_agent(peer);
	peer_agent.handle_signal_packet(packet);
}

void JuiceManager::send_all(void* data, const size_t size) {
	for (auto& [name, agent] : m_agents) {
		m_logger.debug("Sending message peer {} with status: {}\n", agent->address().b64(), as_string(agent->state()));
		agent->send_message(data, size);
	}
}

juice_state JuiceManager::agent_state(const NetAddress& address) {
	if (auto agent = maybe_get_agent(address)) {
		return agent->state();
	}
	return JUICE_STATE_DISCONNECTED;
}
