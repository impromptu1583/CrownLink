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

struct SignalPacket;

class JuiceAgent {
public:
	JuiceAgent(const SNetAddr& ID, std::string init_message);
	void signal_handler(const SignalPacket& packet);
	void send_message(const std::string& msg);
	void send_message(const char* begin, const size_t size);
	void send_message(Util::MemoryFrame frame);

private:
	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

private:
	friend class JuiceManager;
	juice_state m_p2p_state = JUICE_STATE_DISCONNECTED;
	SNetAddr m_id{};

	juice_agent_t* m_agent = nullptr; // NOTE: agent must be after config for construction order
	char m_sdp[JUICE_MAX_SDP_STRING_LEN]{};
	Logger m_logger;
};

class JuiceManager {
public:
	JuiceAgent* maybe_get_agent(const std::string& id);
	JuiceAgent& ensure_agent(const std::string& id);
	void send_p2p(const std::string& id, const std::string& msg);
	void send_p2p(const std::string& id, Util::MemoryFrame frame);
	void signal_handler(const SignalPacket packet);
	void send_all(const std::string&);
	void send_all(const char* begin, const size_t size);
	void send_all(Util::MemoryFrame frame);
	juice_state peer_status(const SNetAddr& peer_id);

private:
	std::map<std::string, JuiceAgent> m_agents;
	Logger m_logger{g_root_logger, "P2P Manager"};
};

inline HANDLE g_receive_event;
inline JuiceManager g_juice_manager;