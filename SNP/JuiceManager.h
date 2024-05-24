#pragma once
#include "common.h"

enum class JuiceSignal {
	LocalDescription,
	Candidate,
	GatheringDone,
};

inline std::string to_string(JuiceSignal value) {
	switch (value) {
		EnumStringCase(JuiceSignal::LocalDescription);
		EnumStringCase(JuiceSignal::Candidate);
		EnumStringCase(JuiceSignal::GatheringDone);
	}
	return std::to_string((s32)value);
}

inline std::string to_string(juice_state value) {
	switch (value) {
		EnumStringCase(JUICE_STATE_DISCONNECTED);
		EnumStringCase(JUICE_STATE_GATHERING);
		EnumStringCase(JUICE_STATE_CONNECTING);
		EnumStringCase(JUICE_STATE_CONNECTED);
		EnumStringCase(JUICE_STATE_COMPLETED);
		EnumStringCase(JUICE_STATE_FAILED);
	}
	return std::to_string((s32)value);
}

struct SignalPacket;

class JuiceAgent {
public:
	JuiceAgent(const NetAddress& address, std::string init_message);
	~JuiceAgent();

	JuiceAgent(const JuiceAgent&) = delete;
	JuiceAgent& operator=(const JuiceAgent&) = delete;

	void handle_signal_packet(const SignalPacket& packet);
	void send_message(void* data, const size_t size);

private:
	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

private:
	friend class JuiceManager;
	juice_state m_p2p_state = JUICE_STATE_DISCONNECTED;
	NetAddress m_address;

	juice_agent_t* m_agent;
	Logger m_logger;
};

class JuiceManager {
public:
	JuiceManager() = default;
	JuiceAgent* maybe_get_agent(const NetAddress& address);
	JuiceAgent& ensure_agent(const NetAddress& address);
	void handle_signal_packet(const SignalPacket& packet);
	void send_p2p(const NetAddress& address, void* data, size_t size);
	void send_all(void* data, size_t size);
	juice_state peer_status(const NetAddress& peer_id);

private:
	std::map<NetAddress, JuiceAgent> m_agents;
	Logger m_logger{g_root_logger, "P2P Manager"};
};

inline HANDLE g_receive_event;
inline JuiceManager g_juice_manager;