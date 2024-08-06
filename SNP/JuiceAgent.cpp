#include "JuiceAgent.h"

#include <regex>

#include "CrownLink.h"
#include "JuiceManager.h"

JuiceAgent::JuiceAgent(const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials)
    : m_p2p_state(JUICE_STATE_DISCONNECTED), m_address{address} {
    juice_config_t config{
        .concurrency_mode = JUICE_CONCURRENCY_MODE_THREAD,
        .stun_server_host = ice_credentials.stun_host.c_str(),
        .stun_server_port = ice_credentials.stun_port,

        .cb_state_changed = on_state_changed,
        .cb_candidate = on_candidate,
        .cb_gathering_done = on_gathering_done,
        .cb_recv = on_recv,
        .user_ptr = this,
    };

    if (ice_credentials.turn_servers.size()) {
        auto count = std::min((int)ice_credentials.turn_servers.size(), 2);

        juice_turn_server servers[2]{};
        for (int i = 0; i < count; i++) {
            servers[i].host = ice_credentials.turn_servers[i].host.c_str();
            servers[i].username = ice_credentials.turn_servers[i].username.c_str();
            servers[i].password = ice_credentials.turn_servers[i].password.c_str();
            servers[i].port = ice_credentials.turn_servers[i].port;
        }
        config.turn_servers = servers;
        config.turn_servers_count = count;
    }

    m_agent = juice_create(&config);
    mark_active();

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

        auto sdp_message = P2P::JuiceLocalDescription{{m_address}, sdp};
        g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, sdp_message);
        spdlog::trace("Init - local SDP {}", sdp);
        juice_gather_candidates(m_agent);
    }
}

void JuiceAgent::send_signal_ping() {
    if (std::chrono::steady_clock::now() - m_last_ping > 1s) {
        auto ping = P2P::Ping{{m_address}};
        g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, ping);
        m_last_ping = std::chrono::steady_clock::now();
    }
}

bool JuiceAgent::send_message(void* data, size_t size) {
    mark_active();

    switch (m_p2p_state) {
        case JUICE_STATE_DISCONNECTED: {
            try_initialize();
        } break;
        case JUICE_STATE_CONNECTED:
        case JUICE_STATE_COMPLETED: {
            if (juice_send(m_agent, (const char*)data, size) == 0) {
                return true;
            }
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
    return false;
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
            juice_get_selected_candidates(
                agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN
            );

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
    if (!std::regex_match(sdp, std::regex(".+26\\.\\d+\\.\\d+\\.\\d+.+"))) {
        auto candidate_message = P2P::JuiceCandidate{{parent.address()}, sdp};
        g_crown_link->crowserve().send_messages(CrowServe::ProtocolType::ProtocolP2P, candidate_message);
    } else {
        spdlog::info("skipped sending radmin candidate: {}", sdp);
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
