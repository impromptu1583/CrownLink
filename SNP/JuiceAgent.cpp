#include "JuiceAgent.h"

#include <regex>

#include "CrownLink.h"
#include "JuiceManager.h"

JuiceAgent::JuiceAgent(const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials)
    : m_p2p_state(JUICE_STATE_DISCONNECTED), m_address{address} {
    
    memset(&m_config, 0, sizeof(m_config));

    m_config.concurrency_mode = JUICE_CONCURRENCY_MODE_THREAD;
    m_config.stun_server_host = ice_credentials.stun_host.c_str();
    const auto res = std::from_chars(ice_credentials.stun_port.data(), ice_credentials.stun_port.data() + ice_credentials.stun_port.size(),
        m_config.stun_server_port);
    if (res.ec == std::errc::invalid_argument or res.ec == std::errc::result_out_of_range) {
        spdlog::error("Invalid stun port received: {}", ice_credentials.stun_port);
    }

    m_config.cb_state_changed = on_state_changed;
    m_config.cb_candidate = on_candidate;
    m_config.cb_gathering_done = on_gathering_done;
    m_config.cb_recv = on_recv;
    m_config.user_ptr = this;

    if (ice_credentials.turn_servers_count) {

        juice_turn_server servers[2]{};
        for (int i = 0; i < ice_credentials.turn_servers_count; i++) {
            servers[i].host = ice_credentials.turn_servers[i].host.c_str();
            servers[i].username = ice_credentials.turn_servers[i].username.c_str();
            servers[i].password = ice_credentials.turn_servers[i].password.c_str();
            //servers[i].port = ice_credentials.turn_servers[i].port;
            const auto res = std::from_chars(
                ice_credentials.turn_servers[i].port.data(),
                ice_credentials.turn_servers[i].port.data() + ice_credentials.turn_servers[i].port.size(),
                servers[i].port
            );
            if (res.ec == std::errc::invalid_argument or res.ec == std::errc::result_out_of_range) {
                spdlog::error("Invalid turn port received: {}", ice_credentials.turn_servers[i].port);
            }
        }
        m_config.turn_servers = servers;
        m_config.turn_servers_count = ice_credentials.turn_servers_count;
    }

    m_agent = juice_create(&m_config);
    mark_active();

    send_signal_ping();
}

JuiceAgent::~JuiceAgent() {
    spdlog::debug("Agent {} closed", m_address);
    juice_destroy(m_agent);
}

void JuiceAgent::try_initialize() {

    switch (m_p2p_state) {
        case JUICE_STATE_COMPLETED:
        case JUICE_STATE_CONNECTED: {
        } break;

        case JUICE_STATE_GATHERING:
        case JUICE_STATE_CONNECTING: {
            spdlog::debug("[{}] P2P agent init. State: {}", m_address, to_string(m_p2p_state));
        } break;

		case JUICE_STATE_FAILED: {
            spdlog::error("[{}] P2P agent init attempted but agent in failed state", m_address);
        } break;

		case JUICE_STATE_DISCONNECTED: {
            char sdp[JUICE_MAX_SDP_STRING_LEN]{};
            juice_get_local_description(m_agent, sdp, sizeof(sdp));

            auto sdp_message = P2P::JuiceLocalDescription{{m_address}, sdp};
            g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, sdp_message);
            spdlog::trace("[{}] Init - local SDP {}", m_address, sdp);
            juice_gather_candidates(m_agent);
        }
    }
}

void JuiceAgent::reset_agent() {
    juice_destroy(m_agent);
    m_agent = juice_create(&m_config);
}

void JuiceAgent::send_signal_ping() {
    auto ping = P2P::Ping{{m_address}};
    g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, ping);
    spdlog::debug("[{}] Sent signal ping", m_address);
}

bool JuiceAgent::send_message(void* data, size_t size) {
    mark_active();

    switch (m_p2p_state) {
        case JUICE_STATE_CONNECTED:
        case JUICE_STATE_COMPLETED: {
            if (juice_send(m_agent, (const char*)data, size) == 0) {
                return true;
            }
            return false;
        } break;
        case JUICE_STATE_DISCONNECTED: {
            send_signal_ping();
        } break;

        case JUICE_STATE_FAILED:
        case JUICE_STATE_CONNECTING:
        case JUICE_STATE_GATHERING: {
        } break;
    }
    spdlog::debug("[{}] Trying to send message but P2P State was {}", m_address, to_string(m_p2p_state));
    return false;
}

void JuiceAgent::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
    auto& parent = *(JuiceAgent*)user_ptr;
    parent.mark_active();
    parent.m_p2p_state = state;
    spdlog::debug("[{}] new state: {}", parent.address(), to_string(state));
    switch (state) {
        case JUICE_STATE_CONNECTED: {
            //spdlog::info("Initially connected");
        } break;
        case JUICE_STATE_COMPLETED: {
            //spdlog::info("Connection negotiation finished");
            char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            juice_get_selected_candidates(
                agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN
            );

            if (std::string{local}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(JuiceConnectionType::Relay);
                spdlog::warn("[{}] Local connection is relayed, performance may be affected", parent.address());
            }
            if (std::string{remote}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(JuiceConnectionType::Relay);
                spdlog::warn("[{}] Remote connection is relayed, performance may be affected", parent.address());
            }
            if (std::regex_match(local, std::regex(".+26.\\d+.\\d+.\\d+.+"))) {
                parent.set_connection_type(JuiceConnectionType::Radmin);
                spdlog::warn("[{}] CrownLink is connected over Radmin - performance will be worse than peer-to-peer", parent.address());
            }
            spdlog::info("[{}] Final candidates were local: {} remote: {}", parent.address(), local, remote);
        } break;
        case JUICE_STATE_FAILED: {
            spdlog::error("[{}] Could not connect, gave up", parent.address());
        } break;
    }
}

void JuiceAgent::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr) {
    auto& parent = *(JuiceAgent*)user_ptr;
    parent.mark_active();
    if (!std::regex_match(sdp, std::regex(".+26\\.\\d+\\.\\d+\\.\\d+.+"))) {
        auto candidate_message = P2P::JuiceCandidate{{parent.address()}, sdp};
        g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, candidate_message);
    } else {
        spdlog::info("[{}] skipped sending radmin candidate: {}", parent.address(), sdp);
    }
}

void JuiceAgent::on_gathering_done(juice_agent_t* agent, void* user_ptr) {
    auto& parent = *(JuiceAgent*)user_ptr;
    parent.mark_active();
    auto done_message = P2P::JuiceDone{{parent.address()}};
    g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, done_message);
}

void JuiceAgent::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
    auto& parent = *(JuiceAgent*)user_ptr;
    parent.mark_active();
    g_crown_link->receive_queue().enqueue(GamePacket{parent.m_address, data, size});
    SetEvent(g_receive_event);
}
