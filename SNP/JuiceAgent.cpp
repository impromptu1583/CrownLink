#include "JuiceAgent.h"
#include "CrowServeManager.h"
#include "Globals.h"

#include <algorithm>

JuiceAgent::JuiceAgent(AgentPair& parent, const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials, JuiceAgentType agent_type)
    : m_parent{parent}, m_p2p_state{JUICE_STATE_DISCONNECTED}, m_address{address}, m_agent_type{agent_type} {
    memset(&m_config, 0, sizeof(m_config));

    m_config.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;
    m_config.stun_server_host = ice_credentials.stun_host.c_str();
    const auto res = std::from_chars(
        ice_credentials.stun_port.data(), ice_credentials.stun_port.data() + ice_credentials.stun_port.size(),
        m_config.stun_server_port
    );
    if (res.ec == std::errc::invalid_argument or res.ec == std::errc::result_out_of_range) {
        spdlog::error("Invalid stun port received: {}", ice_credentials.stun_port);
    }

    m_config.cb_state_changed = on_state_changed;
    m_config.cb_candidate = on_candidate;
    m_config.cb_gathering_done = on_gathering_done;
    m_config.cb_recv = on_recv;
    m_config.user_ptr = this;

    if (ice_credentials.turn_servers_count) {
        for (u32 i = 0; i < ice_credentials.turn_servers_count; i++) {
            m_servers[i].host = ice_credentials.turn_servers[i].host.c_str();
            m_servers[i].username = ice_credentials.turn_servers[i].username.c_str();
            m_servers[i].password = ice_credentials.turn_servers[i].password.c_str();
            const auto result = std::from_chars(
                ice_credentials.turn_servers[i].port.data(),
                ice_credentials.turn_servers[i].port.data() + ice_credentials.turn_servers[i].port.size(),
                m_servers[i].port
            );
            if (result.ec == std::errc::invalid_argument or result.ec == std::errc::result_out_of_range) {
                spdlog::error("Invalid turn port received: {}", ice_credentials.turn_servers[i].port);
            }
        }
        m_config.turn_servers = m_servers;
        m_config.turn_servers_count = ice_credentials.turn_servers_count;
    }

    m_agent = juice_create(&m_config);

    m_controlling = g_context->crowserve().socket().id() < address;

    send_connection_request();
}

JuiceAgent::~JuiceAgent() {
    spdlog::info("[{}] Agent closed", m_address);
    juice_destroy(m_agent);
}

void JuiceAgent::try_initialize(const std::unique_lock<std::shared_mutex>& lock) {
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
            reset_agent(lock);
        }
            [[fallthrough]];

        case JUICE_STATE_DISCONNECTED: {
            if (m_last_local_sdp.empty()) {
                char sdp[JUICE_MAX_SDP_STRING_LEN]{};
                juice_get_local_description(m_agent, sdp, sizeof(sdp));
                m_last_local_sdp = sdp;
            }

            auto sdp_message = P2P::JuiceLocalDescription{{m_address, m_agent_type}, m_last_local_sdp};
            auto& socket = g_context->crowserve().socket();
            socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, sdp_message);
            spdlog::debug("[{}] Init - local SDP {}", m_address, m_last_local_sdp);
            juice_gather_candidates(m_agent);
        }
    }
}

void JuiceAgent::reset_agent(const  std::unique_lock<std::shared_mutex>& lock) {
    spdlog::debug("[{}] resetting agent", m_address);
    juice_destroy(m_agent);
    m_attempt_counter_ours++;
    m_last_local_sdp.clear();
    m_last_remote_sdp.clear();
    m_p2p_state = JUICE_STATE_DISCONNECTED;
    m_agent = juice_create(&m_config);
    spdlog::debug("[{}] P2P agent new state: {}", m_address, to_string(m_p2p_state));
}

void JuiceAgent::send_connection_request() {
    auto connection_request = P2P::ConnectionRequest{{m_address, m_agent_type}, m_attempt_counter_ours};
    auto& socket = g_context->crowserve().socket();
    socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, connection_request);
    spdlog::debug("[{}] Sent connection request, counter: {}", m_address, m_attempt_counter_ours.load());
}

void JuiceAgent::set_agent_type(JuiceAgentType agent_type) {
    std::unique_lock lock{m_mutex};
    m_agent_type = agent_type;
    reset_agent(lock);
}

bool JuiceAgent::is_active() {
    std::shared_lock lock{m_mutex};
    return std::chrono::steady_clock::now() - m_last_active < 2min;
}

bool JuiceAgent::send_message(const char* data, size_t size) {
    std::unique_lock lock{m_mutex};
    mark_active(lock);

    switch (m_p2p_state) {
        case JUICE_STATE_CONNECTED:
        case JUICE_STATE_COMPLETED: {
            return juice_send(m_agent, data, size) == 0;
        }
        case JUICE_STATE_DISCONNECTED: {
            send_connection_request();
        } break;

        case JUICE_STATE_FAILED: {
            spdlog::info("p2p send reset agent");
            reset_agent(lock);
        } break;
        case JUICE_STATE_CONNECTING:
        case JUICE_STATE_GATHERING: {
        } break;
    }
    spdlog::debug("[{}] Trying to send message but P2P State was {}", m_address, to_string(m_p2p_state));
    return false;
}

bool JuiceAgent::send_custom_message(GamePacketSubType sub_type, const char* data, size_t data_size) {
    if (data_size > MAX_PAYLOAD_SIZE) return false;
    if (m_p2p_state != JUICE_STATE_CONNECTED && m_p2p_state != JUICE_STATE_COMPLETED) return false;
    GamePacketData packet_data{{
        .size = u16(data_size + sizeof(GamePacketHeader)),
        .type = GamePacketType::CrownLink,
        .sub_type = sub_type,
    }};
    std::copy_n(data, data_size, packet_data.payload);

    std::unique_lock lock{m_mutex};
    mark_active(lock);
    return juice_send(m_agent, (const char*)&packet_data, packet_data.header.size) == 0;
}

static s64 now_ms() {
    auto now = std::chrono::steady_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch());
    return duration.count();
}

void JuiceAgent::send_ping() {
    auto timestamp = now_ms();
    s64 network_timestamp = htonll(timestamp);
    send_custom_message(
        GamePacketSubType::Ping, reinterpret_cast<const char*>(&network_timestamp), sizeof(network_timestamp)
    );
}

void JuiceAgent::handle_ping(const GamePacket& game_packet) {
    auto payload_size = game_packet.data.header.size - sizeof(GamePacketHeader);
    CustomPacketData response{
        m_address, m_agent_type, GamePacketSubType::PingResponse, game_packet.data.payload, payload_size
    };
    // This is called from a callback, so queue the response instead of sending
    // directly to avoid mutex contention
    g_context->juice_manager().queue_custom_packet(response);
}

void JuiceAgent::handle_ping_response(const GamePacket& game_packet) {
    auto payload_size = game_packet.data.header.size - sizeof(GamePacketHeader);

    if (payload_size != sizeof(s64)) {
        spdlog::warn("[{}] Invalid ping response payload size: {}", m_address, payload_size);
        return;
    }

    const auto network_timestamp = *(s64*)game_packet.data.payload;

    auto timestamp = ntohll(network_timestamp);

    auto current_time = now_ms();
    auto delta = current_time - timestamp;
    if (delta < 0 || delta > 30000) {
        spdlog::warn("[{}] Invalid ping timestamp delta: {}ms", m_address, delta);
        return;
    }
    m_average_latency.update(static_cast<f32>(delta));

    spdlog::debug(
        "[{}][{}] ping: {}ms, average rtt: {}ms",
        m_address, to_string(m_agent_type), delta, (f32)m_average_latency);
}

void JuiceAgent::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
    auto& parent = *static_cast<JuiceAgent*>(user_ptr);
    spdlog::debug("[{}] new state: {}", parent.address(), to_string(state));

    parent.m_p2p_state = state;
    switch (state) {
        case JUICE_STATE_COMPLETED: {
            char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
            juice_get_selected_candidates(
                agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN
            );

            if (std::string{local}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(ConnectionState::Relay);
                spdlog::warn("[{}] Local connection is relayed, performance may be affected", parent.address());
            }
            if (std::string{remote}.find("typ relay") != std::string::npos) {
                parent.set_connection_type(ConnectionState::Relay);
                spdlog::warn("[{}] Remote connection is relayed, performance may be affected", parent.address());
            }
            if (std::regex_match(local, std::regex(".+26.\\d+.\\d+.\\d+.+"))) {
                parent.set_connection_type(ConnectionState::Radmin);
                spdlog::warn(
                    "[{}] CrownLink is connected over Radmin - performance will be worse than peer-to-peer",
                    parent.address()
                );
            }
            spdlog::info("[{}] Final candidates were local: {} remote: {}", parent.address(), local, remote);
        } break;
        case JUICE_STATE_FAILED: {
            spdlog::error("[{}] Could not establish P2P connection", parent.address());
        } break;
        case JUICE_STATE_GATHERING: {
            parent.m_attempt_counter_ours++;
        } break;
    }
}

void JuiceAgent::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr) {
    const auto& parent = *static_cast<JuiceAgent*>(user_ptr);
    if (parent.should_use_candidate(sdp)) {
        auto candidate_message = P2P::JuiceCandidate{{parent.address(), parent.m_agent_type}, sdp};
        auto& socket = g_context->crowserve().socket();
        socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, candidate_message);
    }
}

void JuiceAgent::on_gathering_done(juice_agent_t* agent, void* user_ptr) {
    const auto& parent = *static_cast<JuiceAgent*>(user_ptr);
    auto done_message = P2P::JuiceDone{{parent.address(), parent.m_agent_type}};
    auto& socket = g_context->crowserve().socket();
    socket.send_messages(CrowServe::ProtocolType::ProtocolP2P, done_message);
    spdlog::info("[{}] Gathering done", parent.address());
}

void JuiceAgent::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
    auto& parent = *static_cast<JuiceAgent*>(user_ptr);
    auto packet = GamePacket{parent.m_address, data, size};
    auto& header = packet.data.header;
    if (header.type == GamePacketType::CrownLink) {
        if (header.sub_type == GamePacketSubType::Ping) {
            parent.handle_ping(packet);
        } else if (header.sub_type == GamePacketSubType::PingResponse) {
            parent.handle_ping_response(packet);
        }
        return;
    }

    auto resend_requested = (u8)packet.data.header.flags & (u8)GamePacketFlags::ResendRequest;
    parent.m_parent.update_quality(resend_requested);

    if (g_context) {
        g_context->receive_queue().enqueue(packet);
        SetEvent(g_context->receive_event());
    }
}

bool JuiceAgent::is_radmin_candidate(const std::string& candidate) const {
    static const std::regex radmin_regex{".+26\\.\\d+\\.\\d+\\.\\d+.+"};
    return std::regex_match(candidate, radmin_regex);
}

bool JuiceAgent::is_relay_candidate(const std::string& candidate) const {
    return candidate.find("typ relay") != std::string::npos;
}

bool JuiceAgent::should_use_candidate(const std::string& candidate) const {
    if (is_radmin_candidate(candidate)) return false;
    auto is_relay = is_relay_candidate(candidate);
    if (m_agent_type == JuiceAgentType::Relay) return is_relay;
    if (m_agent_type == JuiceAgentType::P2P) return !is_relay;
    return true;
}
