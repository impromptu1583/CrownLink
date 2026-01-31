#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>

#include "JuiceAgent.h"
#include "../types.h"
#include "../NetShared/StormTypes.h"
#include "Logger.h"
#include "../NetworkQuality.h"

#include <concurrentqueue.h>

enum class SendStrategy{
    PreferP2P,
    PreferRelay,
    Best,
    Redundant,
    AdaptiveRedundant,
};

struct CustomPacketData {
    NetAddress address;
    JuiceAgentType agent_type;
    GamePacketSubType sub_type;
    std::array<char, MAX_PAYLOAD_SIZE> data;
    size_t data_size;

    CustomPacketData() = default;
    CustomPacketData(
        const NetAddress& addr, JuiceAgentType type, GamePacketSubType subtype, const char* payload, size_t size
    )
        : address(addr), agent_type(type), sub_type(subtype), data_size(size) {
        if (size <= MAX_PAYLOAD_SIZE) {
            std::copy_n(payload, size, data.data());
        }
    }
};

class AgentPair {
public:
    AgentPair(const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials);
    ~AgentPair();

    AgentPair(const AgentPair&) = delete;
    AgentPair& operator=(const AgentPair&) = delete;

    bool send_p2p(const char* data, size_t size);
    void send_connection_request();
    void set_player_name(const std::string& name);
    void send_custom_packet(JuiceAgentType agent_type, GamePacketSubType sub_type, const char* data, size_t data_size);
    void update_quality(bool resend_requested);

    bool is_active();
    const NetAddress& address() const { return m_address; }

    juice_state best_agent_state();

    template <typename T>
    void handle_crownlink_message(const T& message) {
        JuiceAgentType type = message.header.agent_type;
        switch (message.header.agent_type) { 
            case JuiceAgentType::P2POnly: {
                m_p2p_agent->handle_crownlink_message(message);
            } break;
            case JuiceAgentType::RelayOnly: {
                if (m_relay_agent) m_relay_agent->handle_crownlink_message(message);
            } break;
            case JuiceAgentType::RelayFallback: {
                if (!m_legacy_agent) {
                    spdlog::debug("[{}] Legacy agent detected, adjusting connetion mode");
                    m_legacy_agent = true;
                    m_p2p_agent->set_agent_type(JuiceAgentType::RelayFallback);
                    m_relay_agent.reset();
                }
                m_p2p_agent->handle_crownlink_message(message);
            } break;
        }
    }

private:
    bool send_with_preference(std::unique_ptr<JuiceAgent>& primary, std::unique_ptr<JuiceAgent>& backup, const char* data, size_t size);
    bool send_redundant(const char* data, size_t size);
    bool send_best(const char* data, size_t size);
    bool send_legacy(const char* data, size_t size);
    bool either_agent_state(juice_state state);
    void send_pings();

private:
    NetAddress m_address;
    SendStrategy m_send_strategy = SendStrategy::AdaptiveRedundant;
    u32 m_send_counter = 0;
    EMA m_average_quality{QUALITY_SAMPLES, 1.0f};

    std::unique_ptr<JuiceAgent> m_p2p_agent;
    std::unique_ptr<JuiceAgent> m_relay_agent;
    std::atomic<bool> m_legacy_agent = false;
};

class JuiceManager {
public:
    JuiceManager() { start_custom_packet_thread(); }
    ~JuiceManager() { stop_custom_packet_thread(); }

    JuiceManager(const JuiceManager&) = delete;
    JuiceManager& operator=(const JuiceManager&) = delete;

    AgentPair* maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);
    AgentPair& ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);

    void clear_inactive_agents();
    void disconnect_if_inactive(const NetAddress& address);
    bool send_p2p(const NetAddress& address, const char* data, size_t size);
    void send_all(const char* data, size_t size);
    void send_connection_request(const NetAddress& address);
    void set_ice_credentials(const CrownLinkProtocol::IceCredentials& ice_credentials);

    void queue_custom_packet(const CustomPacketData& packet);
    void start_custom_packet_thread();
    void stop_custom_packet_thread();

    template <typename T>
    void handle_crownlink_message(const T& message) {
        std::lock_guard lock{m_mutex};

        auto& peer_agent = ensure_agent(message.header.peer_id, lock);
        peer_agent.handle_crownlink_message(message);
    };

    juice_state lobby_agent_state(const AdFile& ad);
    ConnectionState final_connection_type(const NetAddress& address);

    std::mutex& mutex() { return m_mutex; }

private:
    std::unordered_map<NetAddress, std::unique_ptr<AgentPair>> m_agents;
    std::mutex m_mutex;
    CrownLinkProtocol::IceCredentials m_ice_credentials{};
    
    moodycamel::ConcurrentQueue<CustomPacketData> m_custom_packet_queue;
    std::jthread m_custom_packet_thread;
    std::atomic<bool> m_custom_packet_thread_running = false;
};