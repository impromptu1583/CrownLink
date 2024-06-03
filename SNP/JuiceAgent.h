#pragma once
#include "Common.h"

struct SignalPacket;

struct TurnServer {
	std::string host;
	std::string username;
	std::string password;
	uint16_t    port;
};

class JuiceAgent {
public:
	JuiceAgent(const NetAddress& address, std::vector<TurnServer>& turn_servers, const std::string& init_message = "");
	~JuiceAgent();

	JuiceAgent(const JuiceAgent&) = delete;
	JuiceAgent& operator=(const JuiceAgent&) = delete;

	void handle_signal_packet(const SignalPacket& packet);
	void send_message(void* data, const size_t size);
	void send_signal_ping();

public:
	const NetAddress& address() const { return m_address; }
	juice_state state() const { return m_p2p_state; }
	bool is_active() const { return state() != JUICE_STATE_FAILED && std::chrono::steady_clock::now() - m_last_active < 5min; }
	bool is_relayed() const { return m_is_relayed; }
	bool is_radmin() const { return m_is_radmin; }
	void set_relayed(bool value) { m_is_relayed = value; }
	void set_radmin(bool value) { m_is_radmin = value; }
	void mark_last_signal();

private:
	void mark_active() { m_last_active = std::chrono::steady_clock::now(); }
	void try_initialize();

	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

private:
	bool m_is_relayed = false;
	bool m_is_radmin = false;
	std::chrono::steady_clock::time_point m_last_active;
	std::chrono::steady_clock::time_point m_last_signal;
	std::chrono::steady_clock::time_point m_last_ping = std::chrono::steady_clock::now() - 1s;
	juice_state m_p2p_state = JUICE_STATE_DISCONNECTED;
	NetAddress m_address;
	juice_agent_t* m_agent;
};
