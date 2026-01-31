#pragma once
#include <juice.h>

#include <atomic>
#include <chrono>
#include <memory>
#include <mutex>
#include <regex>
#include <shared_mutex>
#include <string>

#include "../types.h"
#include "Logger.h"
#include "../CrowServe/CrowServe.h"
#include "NetworkQuality.h"

struct TurnServer {
    std::string host;
    std::string username;
    std::string password;
    uint16_t port;
};

inline std::string to_string(juice_state value) {
    switch (value) {
        EnumStringCase(JUICE_STATE_DISCONNECTED);
        EnumStringCase(JUICE_STATE_GATHERING);
        EnumStringCase(JUICE_STATE_CONNECTING);
        EnumStringCase(JUICE_STATE_CONNECTED);
        EnumStringCase(JUICE_STATE_COMPLETED);
        EnumStringCase(JUICE_STATE_FAILED);
    }
    return std::to_string((s32)value);
}

inline std::string to_string(JuiceAgentType type) {
    switch (type) {
        case JuiceAgentType::RelayFallback: return "RelayFallback";
        case JuiceAgentType::P2POnly: return "P2POnly";
        case JuiceAgentType::RelayOnly: return "RelayOnly";
    }
    return std::to_string((s32)type);
}

class AgentPair;

class JuiceAgent {
public:
    JuiceAgent(AgentPair& parent, const NetAddress& address, CrownLinkProtocol::IceCredentials& m_ice_credentials, JuiceAgentType agent_type = JuiceAgentType::RelayFallback);
    ~JuiceAgent();

    JuiceAgent(const JuiceAgent&) = delete;
    JuiceAgent& operator=(const JuiceAgent&) = delete;

    template <typename T>
    void handle_crownlink_message(const T& message) {
        std::unique_lock lock{m_mutex};

        if constexpr (std::is_same_v<T, P2P::ConnectionRequest>) {
            spdlog::debug(
                "[{}] Received Connection Request, counter: {}, ours: {}, processed {} state: {}", m_address,
                message.counter, m_attempt_counter_ours.load(), m_attempt_counter_theirs, to_string(m_p2p_state)
            );
            if (m_p2p_state == JUICE_STATE_FAILED) reset_agent(lock);
            if (m_attempt_counter_theirs > message.counter) {
                spdlog::debug("[{}] Counter lower than processed, old message received?", m_address);
                return;
            }
            if (m_attempt_counter_theirs < message.counter) {
                spdlog::debug("[{}] New connection attempt from peer, resetting session", m_address);
                if (m_p2p_state > JUICE_STATE_DISCONNECTED) reset_agent(lock);
                m_attempt_counter_theirs = message.counter;
            }
            if (!m_controlling) {
                send_connection_request();  // no u
                return;
            }
            try_initialize(lock);
        } else if constexpr (std::is_same_v<T, P2P::JuiceLocalDescription>) {
            spdlog::debug("[{}] Received remote description:\n{}", m_address, message.sdp);

            if (m_last_remote_sdp.empty()) {
                m_last_remote_sdp = message.sdp;
                juice_set_remote_description(m_agent, message.sdp.c_str());
                try_initialize(lock);
                return;
            }
            
            if (m_last_remote_sdp == message.sdp) return;  // Duplicate request
            
            spdlog::debug("[{}] Received remote sdp is different from previously set, resetting", m_address);
            reset_agent(lock);
            send_connection_request();
        } else if constexpr (std::is_same_v<T, P2P::JuiceCandidate>) {
            spdlog::debug("[{}] Received candidate:\n{}", m_address, message.candidate);
            if (should_use_candidate(message.candidate)) {
                juice_add_remote_candidate(m_agent, message.candidate.c_str());
            }
        } else if constexpr (std::is_same_v<T, P2P::JuiceDone>) {
            spdlog::debug("[{}] Remote gathering done", m_address);
            juice_set_remote_gathering_done(m_agent);
        }
    };

    bool send_message(const char* data, const size_t size);
    bool send_custom_message(GamePacketSubType sub_type, const char* data, size_t data_size);
    void send_connection_request();
    const NetAddress& address() const { return m_address; }
    juice_state state() { return m_p2p_state.load(); }
    bool connected() { return m_p2p_state == JUICE_STATE_CONNECTED || m_p2p_state == JUICE_STATE_COMPLETED; }
    f32 average_latency() { return m_average_latency; }
    ConnectionState connection_type() { return m_connection_type.load(); }
    void set_agent_type(JuiceAgentType agent_type);

    bool is_active();
    void send_ping();

    void set_connection_type(ConnectionState ct) { m_connection_type = ct; }

private:
    void mark_active(const std::unique_lock<std::shared_mutex>& lock) { m_last_active = std::chrono::steady_clock::now(); };
    void try_initialize(const std::unique_lock<std::shared_mutex>& lock);
    void reset_agent(const std::unique_lock<std::shared_mutex>& lock);

    void handle_ping(const GamePacket& game_packet);
    void handle_ping_response(const GamePacket& game_packet);

    static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
    static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

    bool is_radmin_candidate(const std::string& candidate) const;
    bool is_relay_candidate(const std::string& candidate) const;
    bool should_use_candidate(const std::string& candidate) const;

private:
    AgentPair& m_parent;
    std::atomic<JuiceAgentType> m_agent_type;

    bool m_is_relayed = false;
    bool m_is_radmin = false;
    bool m_controlling = true;

    std::string m_last_local_sdp;
    std::string m_last_remote_sdp;

    std::atomic<ConnectionState> m_connection_type{ConnectionState::Standard};
    std::atomic<juice_state> m_p2p_state = JUICE_STATE_DISCONNECTED;
    std::atomic<u32> m_attempt_counter_ours = get_tick_count();
    u32 m_attempt_counter_theirs = 0;

    NetAddress m_address;
    juice_config_t m_config;
    juice_turn_server m_servers[2];
    juice_agent_t* m_agent;
    std::shared_mutex m_mutex;

    EMA m_average_latency{LATENCY_SAMPLES};

    std::chrono::steady_clock::time_point m_last_active = std::chrono::steady_clock::now();
};
