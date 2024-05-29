#include "JuiceManager.h"
#include "Signaling.h"
#include "JuiceAgent.h"

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>&) {
	auto it = m_agents.find(address);
	if (it != m_agents.end()) {
		return it->second.get();
	}
	return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>&) {
	if (!m_agents.contains(address)) {
		const auto [it, _] = m_agents.emplace(address, std::make_unique<JuiceAgent>(address,m_turn_servers));
		return *it->second;
	}
	return *m_agents.at(address);
}

void JuiceManager::clear_inactive_agents() {
	std::lock_guard lock{m_mutex};
	std::erase_if(m_agents, [this](const auto& pair) {
		const auto& [_, agent] = pair;
		return agent->state() == JUICE_STATE_DISCONNECTED || agent->state() == JUICE_STATE_FAILED;
	});
}

void JuiceManager::send_p2p(const NetAddress& address, void* data, size_t size) {
	std::lock_guard lock{m_mutex};
	auto& agent = ensure_agent(address, lock);
	agent.send_message(data, size);
}

void JuiceManager::handle_signal_packet(const SignalPacket& packet) {
	const auto& peer = packet.peer_address;
	m_logger.trace("Received message for {}: {}", peer.b64(), packet.data);

	std::lock_guard lock{m_mutex};
	if (packet.message_type == SignalMessageType::JuiceTurnCredentials) {
		try {
			auto json = Json::parse(packet.data);

			auto host = json["server"].get<std::string>();
			auto username = json["username"].get<std::string>();
			auto password = json["password"].get<std::string>();
			auto port = json["port"].get<uint16_t>();
			m_turn_servers.emplace_back(juice_turn_server_t{ host.c_str(),username.c_str(),password.c_str(),port });
			m_logger.debug("TURN server info received: {}",packet.data);
		} catch (std::exception& e) {
			m_logger.error("error loading turn server {}",e.what());
		}
	} else {
		auto& peer_agent = ensure_agent(peer, lock);
		peer_agent.handle_signal_packet(packet);
	}
}

void JuiceManager::send_all(void* data, const size_t size) {
	std::lock_guard lock{m_mutex};
	for (auto& [name, agent] : m_agents) {
		m_logger.debug("Sending message peer {} with status: {}\n", agent->address().b64(), as_string(agent->state()));
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

bool JuiceManager::is_relayed(const NetAddress& address) {
	std::lock_guard lock{m_mutex};
	if (auto agent = maybe_get_agent(address, lock)) {
		return agent->is_relayed;
	}
	return false;
}