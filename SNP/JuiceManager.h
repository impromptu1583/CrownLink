#pragma once
#include "Common.h"
#include "JuiceAgent.h"

struct SignalPacket;

class JuiceManager {
public:
	JuiceManager() = default;

	JuiceAgent* maybe_get_agent(const NetAddress& address, const std::lock_guard<std::mutex>&); 
	JuiceAgent& ensure_agent(const NetAddress& address, const std::lock_guard<std::mutex>&);

	void clear_inactive_agents();
	void handle_signal_packet(const SignalPacket& packet);
	void send_p2p(const NetAddress& address, void* data, size_t size);
	void send_all(void* data, size_t size);
	void send_signal_ping(const NetAddress& address);
	void mark_last_signal(const NetAddress& address);

	juice_state agent_state(const NetAddress& address);
	bool is_relayed(const NetAddress& address);

	std::mutex& mutex() { return m_mutex; }

private:
	std::unordered_map<NetAddress, std::unique_ptr<JuiceAgent>> m_agents;
	std::mutex m_mutex;
	std::vector<TurnServer> m_turn_servers;
};