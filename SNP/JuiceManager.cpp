#include "JuiceManager.h"
#include "Signaling.h"
#include "JuiceAgent.h"

JuiceAgent* JuiceManager::maybe_get_agent(const NetAddress& address) {
	auto it = m_agents.find(address);
	if (it != m_agents.end()) {
		return it->second.get();
	}
	return nullptr;
}

JuiceAgent& JuiceManager::ensure_agent(const NetAddress& address) {
	if (!m_agents.contains(address)) {
		const auto [it, _] = m_agents.emplace(address, std::make_unique<JuiceAgent>(address));
		return *it->second;
	}
	return *m_agents.at(address);
}

void JuiceManager::clear_inactive_agents() {
	std::erase_if(m_agents, [this](const auto& pair) {
		const auto& [_, agent] = pair;
		return agent->state() == JUICE_STATE_DISCONNECTED || agent->state() == JUICE_STATE_FAILED;
	});
}

void JuiceManager::send_p2p(const NetAddress& address, void* data, size_t size) {
	auto& agent = ensure_agent(address);
	agent.send_message(data, size);
}

void JuiceManager::handle_signal_packet(const SignalPacket& packet) {
	const auto& peer = packet.peer_address;
	m_logger.trace("Received message for {}: {}", peer.b64(), packet.data);

	auto& peer_agent = ensure_agent(peer);
	peer_agent.handle_signal_packet(packet);
}

void JuiceManager::send_all(void* data, const size_t size) {
	for (auto& [name, agent] : m_agents) {
		m_logger.debug("Sending message peer {} with status: {}\n", agent->address().b64(), as_string(agent->state()));
		agent->send_message(data, size);
	}
}

juice_state JuiceManager::agent_state(const NetAddress& address) {
	if (auto agent = maybe_get_agent(address)) {
		return agent->state();
	}
	return JUICE_STATE_DISCONNECTED;
}

bool JuiceManager::is_relayed(const NetAddress& address) {
	if (auto agent = maybe_get_agent(address)) {
		return agent->is_relayed;
	}
	return false;
}