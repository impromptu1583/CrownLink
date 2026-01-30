#pragma once
#include <unordered_map>
#include <memory>
#include <mutex>

#include "JuiceAgent.h"
#include "../types.h"
#include "../NetShared/StormTypes.h"
#include "Logger.h"

enum class SendStrategy{
    PreferP2P,
    PreferRelay,
    Best,
    Redundant,
    AdaptiveRedundant,
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

private:
    NetAddress m_address;
    SendStrategy m_send_strategy = SendStrategy::AdaptiveRedundant;

    std::unique_ptr<JuiceAgent> m_p2p_agent;
    std::unique_ptr<JuiceAgent> m_relay_agent;
    std::atomic<bool> m_legacy_agent = false;
};

class JuiceManager {
public:
    AgentPair* maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);
    AgentPair& ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock);

    void clear_inactive_agents();
    void disconnect_if_inactive(const NetAddress& address);
    bool send_p2p(const NetAddress& address, const char* data, size_t size);
    void send_all(const char* data, size_t size);
    void send_connection_request(const NetAddress& address);
    void set_ice_credentials(const CrownLinkProtocol::IceCredentials& ice_credentials);

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
};