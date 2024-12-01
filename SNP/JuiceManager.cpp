#include "JuiceManager.h"
#include "Signaling.h"
#include "JuiceAgent.h"

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>&) {
	if (auto it = m_agents.find(address); it != m_agents.end()) {
		return it->second.get();
	}
	return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>&, const std::string& init_message) {
	if (auto it = m_agents.find(address); it != m_agents.end()) {
		if (!it->second->is_active()) {
			it->second = std::make_unique<JuiceAgent>(address, m_turn_servers, init_message);
			spdlog::debug("ensure_agent: Agent inactive, New agent created for address {}", address.b64());
		} else if (!init_message.empty()) {
			it->second->handle_signal_packet(SignalPacket{init_message});
		}
		return *it->second;
	}

	spdlog::debug("ensure_agent: No agent existed for address {}, New agent created", address.b64());
	const auto [new_it, _] = m_agents.emplace(address, std::make_unique<JuiceAgent>(address, m_turn_servers, init_message));
	return *new_it->second;
}

void JuiceManager::clear_inactive_agents() {
	std::lock_guard lock{m_mutex};
	std::erase_if(m_agents, [](const auto& pair) {
		const auto& [_, agent] = pair;
		return !agent->is_active(); 
	});
}

void JuiceManager::send_p2p(const NetAddress& address, void* data, size_t size) {
	std::lock_guard lock{m_mutex};
	auto& agent = ensure_agent(address, lock);
	agent.send_message(data, size);
}

void JuiceManager::send_signal_ping(const NetAddress& address) {
	std::lock_guard lock{m_mutex};
	auto& agent = ensure_agent(address, lock);
	agent.send_signal_ping();
}

void JuiceManager::handle_signal_packet(const SignalPacket& packet) {
	const auto& peer = packet.peer_address;
	spdlog::trace("Received message for {}: {}", peer.b64(), packet.data);

	std::lock_guard lock{m_mutex};
	if (packet.message_type == SignalMessageType::JuiceTurnCredentials) {
		try {
			auto json = Json::parse(packet.data);
			const auto host = json["server"].get<std::string>();
			const auto username = json["username"].get<std::string>();
			const auto password = json["password"].get<std::string>();
			const auto port = json["port"].get<uint16_t>();

			m_turn_servers.emplace_back(host, username, password, port);
			spdlog::debug("TURN server info received: {}", packet.data);
		} catch (const std::exception& e) {
			spdlog::dump_backtrace();
			spdlog::error("error loading turn server {}", e.what());
		}
	} else {
		auto& peer_agent = ensure_agent(peer, lock);
		const auto peer_state = peer_agent.state();
		if (packet.message_type == SignalMessageType::JuiceLocalDescription &&
				(peer_state == JUICE_STATE_FAILED ||
					peer_state == JUICE_STATE_CONNECTED ||
					peer_state == JUICE_STATE_COMPLETED)) {
			auto num_erased = m_agents.erase(peer);
			spdlog::info("New connection requested for {} but existing connection already existed in state {}. Erased {}.",
				peer.b64(), to_string(peer_state), num_erased);
			ensure_agent(peer, lock);
		}
		peer_agent.handle_signal_packet(packet);
	}
}

void JuiceManager::send_all(void* data, const size_t size) {
	std::lock_guard lock{m_mutex};
	for (const auto& [name, agent] : m_agents) {
		spdlog::debug("Sending message peer {} with status: {}\n", agent->address().b64(), as_string(agent->state()));
		agent->send_message(data, size);
	}
}

juice_state JuiceManager::agent_state(const NetAddress& address) {
	std::lock_guard lock{m_mutex};
	if (auto agent = maybe_get_agent(address, lock)) {
		return agent->state();
	}
	return JUICE_STATE_DISCONNECTED;
}

JuiceConnectionType JuiceManager::final_connection_type(const NetAddress& address) {
	std::lock_guard lock{m_mutex};
	if (auto agent = maybe_get_agent(address, lock)) {
		return agent->connection_type();
	}
	return JuiceConnectionType::Standard;
}
