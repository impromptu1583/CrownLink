#include "JuiceManager.h"
#include "signaling.h"

JuiceWrapper::JuiceWrapper(const SNetAddr& id, std::string init_message = "")
: m_p2p_state(JUICE_STATE_DISCONNECTED), m_ID{id}, m_ID_b64{id.b64()}, m_config{
	.stun_server_host = StunServer,
	.stun_server_port = StunServerPort,
	
	.cb_state_changed = on_state_changed,
	.cb_candidate = on_candidate,
	.cb_gathering_done = on_gathering_done,
	.cb_recv = on_recv,
	.user_ptr = this,
}, m_agent{juice_create(&m_config)} {
	if (!init_message.empty()) {
		signal_handler(init_message);
	}
	juice_get_local_description(m_agent, m_sdp, sizeof(m_sdp));

	g_signaling_socket.send_packet(m_ID, SignalMessageType::JuiceLocalDescription, m_sdp);
	g_root_logger.trace("[P2P Agent][{}] Init - local SDP {}", m_ID_b64, m_sdp);
	juice_gather_candidates(m_agent);
}

void JuiceWrapper::signal_handler(const SignalPacket& packet) {
	switch (packet.message_type) {
		case SignalMessageType::JuiceLocalDescription: {
			juice_set_remote_description(m_agent, packet.data.c_str());
			g_root_logger.trace("[P2P Agent][{}] received remote description:\n{}", m_ID_b64, packet.data);
		} break;
		case SignalMessageType::JuciceCandidate: {
			juice_add_remote_candidate(m_agent, packet.data.c_str());
			g_root_logger.trace("[P2P Agent][{}] received remote candidate {}", m_ID_b64, packet.data);
		} break;
		case SignalMessageType::JuiceDone: {
			juice_set_remote_gathering_done(m_agent);
			g_root_logger.trace("[P2P Agent][{}] remote gathering done", m_ID_b64);
		} break;
	}
}

void JuiceWrapper::send_message(const std::string& msg) {
	if (m_p2p_state == JUICE_STATE_CONNECTED || m_p2p_state == JUICE_STATE_COMPLETED) {
		g_root_logger.trace("[P2P Agent][{}] sending message {}", m_ID_b64, msg);
		juice_send(m_agent, msg.c_str(), msg.size());
	} else if (m_p2p_state == JUICE_STATE_FAILED) {
		g_root_logger.error("[P2P Agent][{}] trying to send message but P2P connection failed");
	}
	// TODO: Error handling
}

void JuiceWrapper::send_message(const char* begin, const size_t size) {
	if (m_p2p_state == JUICE_STATE_CONNECTED || m_p2p_state == JUICE_STATE_COMPLETED) {
		g_root_logger.trace("[P2P Agent][{}] sending message {}", m_ID_b64, std::string{begin, size});
		juice_send(m_agent, begin, size);
	} else if (m_p2p_state == JUICE_STATE_FAILED) {
		g_root_logger.error("[P2P Agent][{}] trying to send message but P2P connection failed");
	}
}

void JuiceWrapper::send_message(Util::MemoryFrame frame) {
	send_message((char*)frame.begin(), frame.size());
}

void JuiceWrapper::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->m_p2p_state = state;
	if (state == JUICE_STATE_CONNECTED) {
		g_root_logger.info("[P2P Agent][{}] Initially connected", parent->m_ID_b64);
	} else if (state == JUICE_STATE_COMPLETED) {
		g_root_logger.info("[P2P Agent][{}] Connecttion negotiation finished", parent->m_ID_b64);
		char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN);

		if (std::string{local}.find("typ relay") != std::string::npos) {
			g_root_logger.warn("[P2P Agent][{}] local connection is relayed, performance may be affected", parent->m_ID_b64);
		}
		if (std::string{remote}.find("typ relay") != std::string::npos) {
			g_root_logger.warn("[P2P Agent][{}] remote connection is relayed, performance may be affected", parent->m_ID_b64);
		}
		g_root_logger.trace("[P2P Agent][{}] Final candidates were local: {} remote: {}", parent->m_ID_b64, local, remote);
	} else if (state == JUICE_STATE_FAILED) {
		g_root_logger.error("[P2P Agent][{}] Could not connect, gave up", parent->m_ID_b64);
	}
}

void JuiceWrapper::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr){
	auto& parent = *(JuiceWrapper*)user_ptr;
	g_signaling_socket.send_packet(parent.m_ID, SignalMessageType::JuciceCandidate, sdp);

}

void JuiceWrapper::on_gathering_done(juice_agent_t* agent, void* user_ptr){
	auto& parent = *(JuiceWrapper*)user_ptr;
	g_signaling_socket.send_packet(parent.m_ID, SignalMessageType::JuiceDone);
}

void JuiceWrapper::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	receive_queue.emplace(GamePacket{ parent->m_ID,data,size });
	SetEvent(g_receive_event);
	g_root_logger.trace("[P2P Agent][{}] received: {}", parent->m_ID_b64, std::string{data, size});
}

void JuiceMAN::create_if_not_exist(const std::string& id) {
	if (!m_agents.contains(id)) {
		m_agents.emplace(id, JuiceWrapper{id});
	}
}

void JuiceMAN::send_p2p(const std::string& ID, const std::string& msg) {
	// TODO: error handling
	create_if_not_exist(ID);
	m_agents[ID].send_message(msg);
}

void JuiceMAN::send_p2p(const std::string& ID, Util::MemoryFrame frame) {
	// TODO: error handling
	create_if_not_exist(ID);
	m_agents[ID].send_message(frame);
}

void JuiceMAN::signal_handler(const SignalPacket packet) {
	auto peer_string = std::string((char*)packet.peer_id.address, sizeof(SNetAddr));
	g_root_logger.trace("[P2P Manager] received message for {}: {}",peer_string,packet.data);
	if (!m_agents.contains(peer_string)) {
		m_agents.emplace(peer_string, JuiceWrapper{peer_string, packet.data});
	}
	m_agents[peer_string].signal_handler(packet);
}

void JuiceMAN::send_all(const std::string& msg) {
	for (auto& [name, agent] : m_agents) {
		std::cout << "sending message peer " << (char*)agent.m_ID.address << " with status:" << agent.m_p2p_state << "\n";
		agent.send_message(msg);
	}
}

void JuiceMAN::send_all(const char* begin, const size_t size) {
	for (auto& [name, agent] : m_agents) {
		std::cout << "sending message peer " << (char*)agent.m_ID.address << " with status:" << agent.m_p2p_state << "\n";
		agent.send_message(begin,size);
	}
}
void JuiceMAN::send_all(Util::MemoryFrame frame) {
	for (auto& [name, agent] : m_agents) {
		std::cout << "sending message peer " << (char*)agent.m_ID.address << " with status:" << agent.m_p2p_state << "\n";
		agent.send_message(frame);
	}
}

juice_state JuiceMAN::peer_status(SNetAddr peer_ID) {
	// NOTE: It's ugly but I'm still working on changing out strings for SNetAddr
	auto id = std::string((char*)peer_ID.address, sizeof(SNetAddr));
	if (m_agents.contains(id)) {
		return m_agents[id].m_p2p_state;
	}
	return juice_state(JUICE_STATE_DISCONNECTED);
}
