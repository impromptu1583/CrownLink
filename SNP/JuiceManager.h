#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>

#include "JuiceAgent.h"
#include "../types.h"
#include "../NetShared/StormTypes.h"
#include "Logger.h"
#include "EMA.h"

#include <concurrentqueue.h>

static constexpr auto PING_EVERY = 15;
static constexpr auto QUALITY_SAMPLES = 50;
static constexpr f32 DUPLICATE_SEND_THRESHOLD = 0.8;
static constexpr f32 LATENCY_THRESHOLD = 10.0f;
static constexpr f32 MAXIMUM_LATENCY_FOR_REDUNDANT = 1000 / 8 * 5;  // turns/sec * 5

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
    void send_custom_packet(JuiceAgentType agent_type, GamePacketSubType sub_type, const char* data, size_t data_size);
    void update_quality(bool resend_requested);

    bool is_active();
    const NetAddress& address() const { return m_address; }

    juice_state best_agent_state();

    template <typename T>
    void handle_crownlink_message(const T& message) {
        switch (message.header.agent_type) { 
            case JuiceAgentType::P2P: {
                m_p2p_agent->handle_crownlink_message(message);
            } break;
            case JuiceAgentType::Relay: {
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
    std::unique_ptr<JuiceAgent>& get_best();
    bool send_best(const char* data, size_t size);
    bool send_legacy(const char* data, size_t size);
    bool should_send_redundant();
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
    JuiceManager() {
        m_custom_packet_ready = CreateEvent(nullptr, FALSE, FALSE, nullptr);
        start_custom_packet_thread();
    }
    ~JuiceManager() {
        stop_custom_packet_thread();
        CloseHandle(m_custom_packet_ready);
    }

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
    HANDLE m_custom_packet_ready;
    std::jthread m_custom_packet_thread;
    std::atomic<bool> m_custom_packet_thread_running = false;
};