#include "JuiceManager.h"

AgentPair::AgentPair(const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials) {
    m_p2p_agent = std::make_unique<JuiceAgent>(address, ice_credentials, JuiceAgentType::P2POnly);
    m_relay_agent = std::make_unique<JuiceAgent>(address, ice_credentials, JuiceAgentType::RelayOnly);
}

AgentPair::~AgentPair() {}

bool AgentPair::send_with_preference(std::unique_ptr<JuiceAgent>& primary, std::unique_ptr<JuiceAgent>& backup, const char* data, size_t size) {
    if (primary && primary->connected()) {
        auto result = primary->send_message(data, size);
        if (result) return result;
    }
    if (backup && backup->connected()) {
        return backup->send_message(data, size);
    }
    return false;
}

bool AgentPair::send_redundant(const char* data, size_t size) {
    auto p2p_sent = false;
    auto relay_sent = false;
    if (m_p2p_agent && m_p2p_agent->connected()) {
        p2p_sent = m_p2p_agent->send_message(data, size);
    }
    if (m_relay_agent && m_relay_agent->connected()) {
        relay_sent = m_relay_agent->send_message(data, size);
    }
    return p2p_sent || relay_sent;
}

bool AgentPair::send_best(const char* data, size_t size) {
    // TODO
    return false;
}

bool AgentPair::send_legacy(const char* data, size_t size) {
    if (m_p2p_agent && m_p2p_agent->connected()) {
        return m_p2p_agent->send_message(data, size);
    }
    return false;
}

bool AgentPair::send_p2p(const NetAddress& address, const char* data, size_t size) {
    if (m_legacy_agent) return send_legacy(data, size);

    switch (m_send_strategy) {
        case SendStrategy::PreferP2P: {
            return send_with_preference(m_p2p_agent, m_relay_agent, data, size);
        } break;
        case SendStrategy::PreferRelay: {
            return send_with_preference(m_relay_agent, m_p2p_agent, data, size);
        } break;
        case SendStrategy::Best: {
            // TODO switch to send_best when implemented
            return send_redundant(data, size);
        } break;
        case SendStrategy::Redundant: {
            return send_redundant(data, size);
        } break;
        case SendStrategy::AdaptiveRedundant: {
            // TODO send_best or send_redundant depending on quality
            return send_redundant(data, size);
        } break;
    }
}

void AgentPair::send_connection_request(const NetAddress& address) {
    if (m_p2p_agent) m_p2p_agent->send_connection_request();
    if (m_relay_agent) m_relay_agent->send_connection_request();
}

bool AgentPair::either_agent_state(juice_state state) {
    if (m_p2p_agent && m_p2p_agent->state() == state) return true;
    if (m_relay_agent && m_relay_agent->state() == state) return true;
    return false;
}

juice_state AgentPair::best_agent_state() {
    if (either_agent_state(JUICE_STATE_COMPLETED)) return JUICE_STATE_COMPLETED;
    if (either_agent_state(JUICE_STATE_CONNECTED)) return JUICE_STATE_CONNECTED;
    if (either_agent_state(JUICE_STATE_CONNECTING)) return JUICE_STATE_CONNECTING;
    if (either_agent_state(JUICE_STATE_GATHERING)) return JUICE_STATE_GATHERING;
    if (either_agent_state(JUICE_STATE_DISCONNECTED)) return JUICE_STATE_DISCONNECTED;
    return JUICE_STATE_FAILED;
}

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock) {
    if (const auto it = m_agents.find(address); it != m_agents.end()) {
        return it->second.get();
    }
    return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock) {
    if (const auto it = m_agents.find(address); it != m_agents.end()) {
        return *it->second;
    }

    const auto [new_it, _] = m_agents.emplace(address, std::make_unique<JuiceAgent>(address, m_ice_credentials));
    return *new_it->second;
}

void JuiceManager::clear_inactive_agents() {
    std::lock_guard lock{m_mutex};
    std::erase_if(m_agents, [](const auto& pair) {
        const auto& [_, agent] = pair;
        return !agent->is_active();
    });
}

void JuiceManager::disconnect_if_inactive(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    auto count = std::erase_if(m_agents, [&address](const auto& pair) {
        const auto& [addr, agent] = pair;
        return !agent->is_active() && addr == address;
    });
    if (count) {
        spdlog::debug("Disconnected {} inactive agent with ID {}", count, address);
    }
}

bool JuiceManager::send_p2p(const NetAddress& address, const char* data, size_t size) {
    std::lock_guard lock{m_mutex};
    auto& agent = ensure_agent(address, lock);
    auto success = agent.send_message(data, size);
    return success;
}

void JuiceManager::send_connection_request(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    auto& agent = ensure_agent(address, lock);
    agent.send_connection_request();
}

void JuiceManager::set_ice_credentials(const CrownLinkProtocol::IceCredentials& ice_credentials) {
    std::lock_guard lock{m_mutex};
    m_ice_credentials = ice_credentials;
}

void JuiceManager::send_all(const char* data, const size_t size) {
    std::lock_guard lock{m_mutex};
    for (auto& [name, agent] : m_agents) {
        spdlog::debug("Sending message peer {} with status: {}\n", agent->address(), to_string(agent->state()));
        agent->send_message(data, size);
    }
}

juice_state JuiceManager::lobby_agent_state(const AdFile& ad) {
    std::lock_guard lock{m_mutex};
    auto& agent = ensure_agent(ad.game_info.host, lock);
    agent.set_player_name(ad.game_info.game_name);
    return agent.state();
}

ConnectionState JuiceManager::final_connection_type(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    if (auto agent = maybe_get_agent(address, lock)) {
        return agent->connection_type();
    }
    return ConnectionState::Standard;
}