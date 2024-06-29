#include "JuiceAgent.h"

#include "Signaling.h"
#include "JuiceManager.h"
#include "CrownLink.h"
#include <regex>

JuiceAgent::JuiceAgent(const NetAddress& address, std::vector<TurnServer>& turn_servers, const std::string& init_message)
: m_p2p_state(JUICE_STATE_DISCONNECTED), m_address{address} {
	const auto& snp_config = SnpConfig::instance();
	juice_config_t config{
		.concurrency_mode = JUICE_CONCURRENCY_MODE_THREAD,
		.stun_server_host = snp_config.stun_server.c_str(),
		.stun_server_port = snp_config.stun_port,

		.cb_state_changed = on_state_changed,
		.cb_candidate = on_candidate,
		.cb_gathering_done = on_gathering_done,
		.cb_recv = on_recv,
		.user_ptr = this,
	};
	if (!turn_servers.empty()) {
		juice_turn_server servers[5]{};
		for (unsigned int i = 0; i < turn_servers.size() && i < 5; i++) {
			servers[i].host = turn_servers[i].host.c_str();
			servers[i].username = turn_servers[i].username.c_str();
			servers[i].password = turn_servers[i].password.c_str();
			servers[i].port = turn_servers[i].port;
		}
		config.turn_servers = servers;
		config.turn_servers_count = (turn_servers.size() < 5) ? turn_servers.size() : 5;

	}
	m_agent = juice_create(&config);
	mark_active();

	if (!init_message.empty()) {
		handle_signal_packet(SignalPacket{init_message});
	}
	try_initialize();
}

JuiceAgent::~JuiceAgent() {
	spdlog::debug("Agent {} closed", m_address.b64());
    juice_destroy(m_agent);
}

void JuiceAgent::mark_last_signal() {
	mark_active();
	m_last_signal = std::chrono::steady_clock::now();
	try_initialize();
}

void JuiceAgent::try_initialize() {
	send_signal_ping();
	if (m_p2p_state == JUICE_STATE_DISCONNECTED && std::chrono::steady_clock::now() - m_last_signal < 10s) {
		char sdp[JUICE_MAX_SDP_STRING_LEN]{};
		juice_get_local_description(m_agent, sdp, sizeof(sdp));

		g_crown_link->signaling_socket().send_packet(m_address, SignalMessageType::JuiceLocalDescription, sdp);
		spdlog::trace("Init - local SDP {}", sdp);
		juice_gather_candidates(m_agent);
	}
}

void JuiceAgent::send_signal_ping() {
	if (std::chrono::steady_clock::now() - m_last_ping > 1s) {
		g_crown_link->signaling_socket().send_packet(m_address, SignalMessageType::SignalingPing, "");
		m_last_ping = std::chrono::steady_clock::now();
	}
}

void JuiceAgent::handle_signal_packet(const SignalPacket& packet) {
	mark_active();
	mark_last_signal();

	switch (packet.message_type) {
        case SignalMessageType::SignalingPing:{
            spdlog::trace("Received Ping");
        } break;
        case SignalMessageType::JuiceLocalDescription: {
            spdlog::trace("Received remote description:\n{}", packet.data);
            juice_set_remote_description(m_agent, packet.data.c_str());
        } break;
        case SignalMessageType::JuciceCandidate: {
            spdlog::trace("Received remote candidate {}", packet.data);
            juice_add_remote_candidate(m_agent, packet.data.c_str());
        } break;
        case SignalMessageType::JuiceDone: {
            spdlog::trace("Remote gathering done");
            juice_set_remote_gathering_done(m_agent);
        } break;
	}
}

void JuiceAgent::send_message(void* data, size_t size) {
	mark_active();

	switch (m_p2p_state) {
        case JUICE_STATE_DISCONNECTED:{
            try_initialize();
        } break;		
        case JUICE_STATE_CONNECTED:
        case JUICE_STATE_COMPLETED: {
            juice_send(m_agent, (const char*)data, size);
        } break;
        case JUICE_STATE_FAILED: {
            spdlog::dump_backtrace();
            spdlog::error("Trying to send message but P2P connection failed");
        } break;
        default: {
            spdlog::dump_backtrace();
            spdlog::error("Trying to send message but P2P connection is in unexpected state");
        } break;
	}
}

void JuiceAgent::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
	JuiceAgent& parent = *(JuiceAgent*)user_ptr;
	parent.mark_active();
	parent.m_p2p_state = state;
	spdlog::debug("Connection changed state, new state: {}", to_string(state));
	switch (state) {
        case JUICE_STATE_CONNECTED: {
            spdlog::info("Initially connected");
        } break;
        case JUICE_STATE_COMPLETED: {
            spdlog::info("Connection negotiation finished");
            char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN);

            if (std::string{local}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(JuiceConnectionType::Relay);
                spdlog::warn("Local connection is relayed, performance may be affected");
            }
            if (std::string{remote}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(JuiceConnectionType::Relay);
                spdlog::warn("Remote connection is relayed, performance may be affected");
            }
            if (std::regex_match(local, std::regex(".+26.\\d+.\\d+.\\d+.+"))) {
                parent.set_connection_type(JuiceConnectionType::Radmin);
                spdlog::warn("CrownLink is connected over Radmin - performance will be worse than peer-to-peer");
            }
            spdlog::info("Final candidates were local: {} remote: {}", local, remote);
        } break;
        case JUICE_STATE_FAILED: {
            spdlog::dump_backtrace();
            spdlog::error("Could not connect, gave up");
        } break;
	}
}

void JuiceAgent::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr) {
	auto& parent = *(JuiceAgent*)user_ptr;
	parent.mark_active();
	if (!std::regex_match(sdp, std::regex(".+26.\\d+.\\d+.\\d+.+"))) {
		g_crown_link->signaling_socket().send_packet(parent.m_address, SignalMessageType::JuciceCandidate, sdp);
	} else {
		spdlog::info("skipped sending radmin candidate: {}", sdp);
	}
}

void JuiceAgent::on_gathering_done(juice_agent_t* agent, void* user_ptr) {
	auto& parent = *(JuiceAgent*)user_ptr;
	parent.mark_active();
	g_crown_link->signaling_socket().send_packet(parent.m_address, SignalMessageType::JuiceDone);
}

void JuiceAgent::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
	auto& parent = *(JuiceAgent*)user_ptr;
	parent.mark_active();
	//g_crown_link->receive_queue().emplace(GamePacket{parent.m_address, data, size});
	g_crown_link->receive_queue().enqueue(GamePacket{ parent.m_address, data, size });
	SetEvent(g_receive_event);
}
