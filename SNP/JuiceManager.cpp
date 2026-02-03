#include "JuiceManager.h"

AgentPair::AgentPair(const NetAddress& address, CrownLinkProtocol::IceCredentials& ice_credentials) {
    m_p2p_agent = std::make_unique<JuiceAgent>(*this, address, ice_credentials, JuiceAgentType::P2P);
    m_relay_agent = std::make_unique<JuiceAgent>(*this, address, ice_credentials, JuiceAgentType::Relay);
    m_address = address;
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

std::unique_ptr<JuiceAgent>& AgentPair::get_best() {
    if (m_legacy_agent) return m_p2p_agent;
    if (!m_relay_agent || !m_relay_agent->connected()) return m_p2p_agent;
    if (!m_p2p_agent || !m_p2p_agent->connected()) return m_relay_agent;

    auto p2p_rtt = m_p2p_agent->average_latency();
    auto relay_rtt = m_relay_agent->average_latency();
    auto rtt_delta = p2p_rtt - relay_rtt;
    if (std::abs(rtt_delta) < LATENCY_THRESHOLD) return m_p2p_agent;
    if (rtt_delta > 0) return m_relay_agent;
    return m_p2p_agent;
}

bool AgentPair::send_best(const char* data, size_t size) {
    auto& best_agent = get_best();
    if (best_agent == m_p2p_agent) m_stats.p2p++;
    if (best_agent == m_relay_agent) m_stats.relay++;
    if (best_agent) return best_agent->send_message(data, size);
    return false;
}

bool AgentPair::send_legacy(const char* data, size_t size) {
    if (m_p2p_agent && m_p2p_agent->connected()) {
        return m_p2p_agent->send_message(data, size);
    }
    return false;
}

bool AgentPair::should_send_redundant() {
    if (m_legacy_agent) return false;
    if (!m_p2p_agent || !m_p2p_agent->connected() || 
        !m_relay_agent || !m_relay_agent->connected()) return false;

    auto p2p_rtt = m_p2p_agent->average_latency();
    auto relay_rtt = m_relay_agent->average_latency();

    // Don't send redundant if either connection is very high latency as it's likely counterproductive
    if (p2p_rtt > MAXIMUM_LATENCY_FOR_REDUNDANT || relay_rtt > MAXIMUM_LATENCY_FOR_REDUNDANT) return false;

    if (m_average_quality > DUPLICATE_SEND_THRESHOLD) return false;
    
    spdlog::debug(
        "[{}] Sending redundant. Quality: {}, RTT[p2p:{}, relay:{}]",
        m_address, (f32)m_average_quality, p2p_rtt, relay_rtt
    );
    return true;
}

bool AgentPair::send_p2p(const char* data, size_t size) {
    m_send_counter++;
    if (m_send_counter % PING_EVERY == 0) {
        send_pings();
        spdlog::debug("[{}] Average quality: {}", m_address, (f32)m_average_quality);
    }
    if (m_send_counter % STAT_PRINT_EVERY == 0) {
        spdlog::info("[{}] Pair Stats: p2p: {} relay: {} redundant: {} quality: {}", 
            m_address, m_stats.p2p, m_stats.relay, m_stats.redundant, (f32)m_average_quality);
    }
    if (m_legacy_agent) return send_legacy(data, size);

    switch (m_send_strategy) {
        case SendStrategy::PreferP2P: {
            return send_with_preference(m_p2p_agent, m_relay_agent, data, size);
        } break;
        case SendStrategy::PreferRelay: {
            return send_with_preference(m_relay_agent, m_p2p_agent, data, size);
        } break;
        case SendStrategy::Best: {
            return send_best(data, size);
        } break;
        case SendStrategy::Redundant: {
            m_stats.redundant++;
            return send_redundant(data, size);
        } break;
        case SendStrategy::AdaptiveRedundant: {
            if (should_send_redundant()) {
                m_stats.redundant++;
                return send_redundant(data, size);
            }
            return send_best(data, size);
        } break;
    }
    return false;
}

void AgentPair::send_connection_request() {
    if (m_p2p_agent) m_p2p_agent->send_connection_request();
    if (!m_legacy_agent && m_relay_agent) m_relay_agent->send_connection_request();
}

void AgentPair::send_custom_packet(JuiceAgentType agent_type, GamePacketSubType sub_type, const char* data, size_t data_size) {
    switch (agent_type) {
        case JuiceAgentType::P2P:
        case JuiceAgentType::RelayFallback:
            if (m_p2p_agent) m_p2p_agent->send_custom_message(sub_type, data, data_size);
            break;
        case JuiceAgentType::Relay:
            if (m_relay_agent) m_relay_agent->send_custom_message(sub_type, data, data_size);
            break;
    }
}

void AgentPair::update_quality(bool resend_requested) {
    if (resend_requested) {
        m_average_quality.update(0.0f);
    } else {
        m_average_quality.update(1.0f);
    }
}

bool AgentPair::is_active() {
    auto p2p_active = m_p2p_agent && m_p2p_agent->is_active();
    auto relay_active = m_relay_agent && m_relay_agent->is_active();
    return p2p_active || relay_active;
}

bool AgentPair::either_agent_state(juice_state state) {
    if (m_p2p_agent && m_p2p_agent->state() == state) return true;
    if (m_relay_agent && m_relay_agent->state() == state) return true;
    return false;
}

void AgentPair::send_pings() {
    if (m_p2p_agent && m_p2p_agent->connected()) m_p2p_agent->send_ping();
    if (m_relay_agent && m_relay_agent->connected()) m_relay_agent->send_ping();
}

juice_state AgentPair::best_agent_state() {
    if (either_agent_state(JUICE_STATE_COMPLETED)) return JUICE_STATE_COMPLETED;
    if (either_agent_state(JUICE_STATE_CONNECTED)) return JUICE_STATE_CONNECTED;
    if (either_agent_state(JUICE_STATE_CONNECTING)) return JUICE_STATE_CONNECTING;
    if (either_agent_state(JUICE_STATE_GATHERING)) return JUICE_STATE_GATHERING;
    if (either_agent_state(JUICE_STATE_DISCONNECTED)) return JUICE_STATE_DISCONNECTED;
    return JUICE_STATE_FAILED;
}

AgentPair* JuiceManager::maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock) {
    if (const auto it = m_agents.find(address); it != m_agents.end()) {
        return it->second.get();
    }
    return nullptr;
}

AgentPair& JuiceManager::ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>& lock) {
    if (const auto it = m_agents.find(address); it != m_agents.end()) {
        return *it->second;
    }

    const auto [new_it, _] = m_agents.emplace(address, std::make_unique<AgentPair>(address, m_ice_credentials));
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
    auto success = agent.send_p2p(data, size);
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

void JuiceManager::queue_custom_packet(const CustomPacketData& packet) {
    m_custom_packet_queue.enqueue(packet);
    SetEvent(m_custom_packet_ready);
}

void JuiceManager::start_custom_packet_thread() {
    m_custom_packet_thread_running = true;
    m_custom_packet_thread = std::jthread([this](std::stop_token stop) {
        SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_ABOVE_NORMAL);

        CustomPacketData packet;
        while (!stop.stop_requested() && m_custom_packet_thread_running) {
            if (m_custom_packet_queue.try_dequeue(packet)) {
                // Send custom packet through the appropriate agent
                std::lock_guard lock{m_mutex};
                if (auto* agent_pair = maybe_get_agent(packet.address, lock)) {
                    agent_pair->send_custom_packet(
                        packet.agent_type, packet.sub_type, packet.data.data(), packet.data_size
                    );
                }
            } else {
                WaitForSingleObject(m_custom_packet_ready, 5000);
            }
        }
    });
}

void JuiceManager::stop_custom_packet_thread() {
    m_custom_packet_thread_running = false;
    SetEvent(m_custom_packet_ready);
    if (m_custom_packet_thread.joinable()) {
        m_custom_packet_thread.request_stop();
        m_custom_packet_thread.join();
    }
}

void JuiceManager::send_all(const char* data, const size_t size) {
    std::lock_guard lock{m_mutex};
    for (auto& [name, agent] : m_agents) {
        spdlog::debug("Sending message peer {} with status: {}\n", agent->address(), to_string(agent->best_agent_state()));
        agent->send_p2p(data, size);
    }
}

juice_state JuiceManager::lobby_agent_state(const AdFile& ad) {
    std::lock_guard lock{m_mutex};
    auto& agent = ensure_agent(ad.game_info.host, lock);
    return agent.best_agent_state();
}

ConnectionState JuiceManager::final_connection_type(const NetAddress& address) {
    // TODO: Revisit this
    return ConnectionState::Standard;
}