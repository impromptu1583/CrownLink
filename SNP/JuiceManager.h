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
	JuiceAgent(const SNetAddr& ID, std::string init_message);
	~JuiceAgent();
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
	SNetAddr m_id{};

	juice_agent_t* m_agent = nullptr;
	char m_sdp[JUICE_MAX_SDP_STRING_LEN]{};
	Logger m_logger;
};

class JuiceManager {
public:
	JuiceManager() {};
	~JuiceManager();
	JuiceAgent* maybe_get_agent(const std::string& id);
	JuiceAgent& ensure_agent(const std::string& id);
	void handle_signal_packet(const SignalPacket& packet);
	void send_p2p(const std::string& id, void* data, size_t size);
	void send_all(void* data, size_t size);
	juice_state peer_status(const SNetAddr& peer_id);

private:
	std::map<std::string, JuiceAgent> m_agents;
	Logger m_logger{g_root_logger, "P2P Manager"};
};

inline HANDLE g_receive_event;
inline JuiceManager g_juice_manager;