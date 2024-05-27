#pragma once
#include "Common.h"
#include "JuiceAgent.h"

struct SignalPacket;

class JuiceManager {
public:
	JuiceManager() = default;
	JuiceAgent* maybe_get_agent(const NetAddress& address);
	JuiceAgent& ensure_agent(const NetAddress& address);
	void clear_inactive_agents();
	void handle_signal_packet(const SignalPacket& packet);
	void send_p2p(const NetAddress& address, void* data, size_t size);
	void send_all(void* data, size_t size);

	juice_state agent_state(const NetAddress& address);
	bool is_relayed(const NetAddress& address);

private:
	std::unordered_map<NetAddress, std::unique_ptr<JuiceAgent>> m_agents;
	std::mutex m_mutex;
	Logger m_logger{Logger::root(), "P2P Manager"};
};