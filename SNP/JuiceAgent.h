#pragma once
#include "Common.h"

struct SignalPacket;

struct TurnServer {
    std::string host;
    std::string username;
    std::string password;
    uint16_t port;
};

enum class JuiceConnectionType {
    Standard,
    Relay,
    Radmin
};

class JuiceAgent {
public:
    JuiceAgent(const NetAddress& address, CrownLinkProtocol::IceCredentials& m_ice_credentials);
    ~JuiceAgent();

    JuiceAgent(const JuiceAgent&) = delete;
    JuiceAgent& operator=(const JuiceAgent&) = delete;

    template <typename T>
    void handle_crownlink_message(const T& message) {
        std::unique_lock lock{m_mutex};

        if constexpr (std::is_same_v<T, P2P::ConnectionRequest>) {
            spdlog::debug(
                "[{}] Received Connection Request, counter: {}, ours: {}, processed {} state: {}", m_address,
                message.counter, m_connreq_count.load(), m_connreq_processed, to_string(m_p2p_state)
            );
            if (m_connreq_processed > message.counter) {
                spdlog::debug("[{}] counter lower than processed, old message received?", m_address);
                return;
            }
            if (m_connreq_processed < message.counter) {
                // this is a new conn request
                spdlog::debug("[{}] counters didn't match, resetting", m_address);
                reset_agent(lock);
                m_connreq_processed = message.counter;
            } else if (m_p2p_state == JUICE_STATE_FAILED) {
                reset_agent(lock);
            }
            if (!m_controlling) {
                send_connection_request();  // no u
                return;
            }
            try_initialize(lock);
        } else if constexpr (std::is_same_v<T, P2P::JuiceLocalDescription>) {
            spdlog::debug("[{}] Received remote description:\n{}", m_address, message.sdp);
            if (m_remote_description_set) {
                spdlog::debug("[{}] Remote description was set previously, resetting", m_address);
                reset_agent(lock);
                m_connreq_count++;
                send_connection_request();
                return;
            }

            m_remote_description_set = true;
            juice_set_remote_description(m_agent, message.sdp.c_str());
            try_initialize(lock);
        } else if constexpr (std::is_same_v<T, P2P::JuiceCandidate>) {
            spdlog::debug("[{}] Received candidate:\n{}", m_address, message.candidate);
            juice_add_remote_candidate(m_agent, message.candidate.c_str());
        } else if constexpr (std::is_same_v<T, P2P::JuiceDone>) {
            spdlog::debug("[{}] Remote gathering done", m_address);
            juice_set_remote_gathering_done(m_agent);
        }
    };

    bool send_message(void* data, const size_t size);
    void send_connection_request();

public:
    const NetAddress& address() const { return m_address; }
    juice_state state() { return m_p2p_state.load(); }
    JuiceConnectionType connection_type() { return m_connection_type.load(); }
    void set_player_name(const std::string& name);
    void set_player_name(const char game_name[128]);
    std::string& player_name();

    bool is_active();

    void set_connection_type(JuiceConnectionType ct) { m_connection_type = ct; }

private:
    void mark_active(std::unique_lock<std::shared_mutex>& lock) { m_last_active = std::chrono::steady_clock::now(); };
    void try_initialize(std::unique_lock<std::shared_mutex>& lock);
    void reset_agent(std::unique_lock<std::shared_mutex>& lock);

    static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
    static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
    static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
    static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

private:
    bool m_is_relayed = false;
    bool m_is_radmin = false;
    bool m_remote_description_set = false;
    bool m_controlling = true;

    std::atomic<JuiceConnectionType> m_connection_type{JuiceConnectionType::Standard};
    std::atomic<juice_state> m_p2p_state = JUICE_STATE_DISCONNECTED;
    std::atomic<u32> m_connreq_count = get_tick_count();
    u32 m_connreq_processed = 0;

    NetAddress m_address;
    juice_config_t m_config;
    juice_turn_server m_servers[2];
    juice_agent_t* m_agent;
    std::string m_player_name;
    std::shared_mutex m_mutex;

    u32 m_resends_requested = 0;
    u32 m_packet_count = 0;

    std::chrono::steady_clock::time_point m_last_active = std::chrono::steady_clock::now();
};
