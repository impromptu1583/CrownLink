#include "JuiceManager.h"

#include "JuiceAgent.h"

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>&) {
    auto it = m_agents.find(address);
    if (it != m_agents.end()) {
        return it->second.get();
    }
    return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>&) {
    auto it = m_agents.find(address);
    if (it != m_agents.end()) {
        if (!it->second->is_active()) {
            it->second = std::make_unique<JuiceAgent>(address, m_ice_credentials);
        }
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

bool JuiceManager::send_p2p(const NetAddress& address, void* data, size_t size) {
    std::lock_guard lock{m_mutex};
    auto&           agent = ensure_agent(address, lock);
    return agent.send_message(data, size);
}

void JuiceManager::send_signal_ping(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    auto&           agent = ensure_agent(address, lock);
    agent.send_signal_ping();
}

void JuiceManager::set_ice_credentials(const CrownLinkProtocol::IceCredentials& ice_credentials) {
    std::lock_guard lock{m_mutex};
    m_ice_credentials = ice_credentials;
}

void JuiceManager::send_all(void* data, const size_t size) {
    std::lock_guard lock{m_mutex};
    for (auto& [name, agent] : m_agents) {
        spdlog::debug("Sending message peer {} with status: {}\n", agent->address().b64(), as_string(agent->state()));
        agent->send_message(data, size);
    }
}

juice_state JuiceManager::agent_state(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    auto&  agent = ensure_agent(address, lock);
    return agent.state();

}

JuiceConnectionType JuiceManager::final_connection_type(const NetAddress& address) {
    std::lock_guard lock{m_mutex};
    if (auto agent = maybe_get_agent(address, lock)) {
        return agent->connection_type();
    }
    return JuiceConnectionType::Standard;
}
