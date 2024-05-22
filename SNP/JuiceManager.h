#pragma once
#include "common.h"

enum class JuiceSignal {
	LocalDescription,
	Candidate,
	GatheringDone,
};

struct SignalPacket;

class JuiceWrapper {
public:
	JuiceWrapper() = default; // This is wrong, but required for std::map's operator[]
	JuiceWrapper(const SNETADDR& ID, std::string init_message);
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
	static constexpr const char* StunServer = "stun.l.google.com";
	static constexpr u16 StunServerPort = 19302;

	friend class JuiceMAN;
	juice_state m_p2p_state = JUICE_STATE_DISCONNECTED;
	SNETADDR m_ID;
	std::string m_ID_b64;

	juice_config_t m_config;
	juice_agent_t* m_agent; // NOTE: agent must be after config for construction order
	char m_sdp[JUICE_MAX_SDP_STRING_LEN];
};

class JuiceMAN {
public:
	void create_if_not_exist(const std::string& ID);
	void send_p2p(const std::string& ID, const std::string& msg);
	void send_p2p(const std::string& ID, Util::MemoryFrame frame);
	void signal_handler(const SignalPacket packet);
	void send_all(const std::string&);
	void send_all(const char* begin, const size_t size);
	void send_all(Util::MemoryFrame frame);
	juice_state peer_status(SNETADDR peer_ID);

private:
	std::map<std::string, JuiceWrapper> m_agents;
};

inline HANDLE g_receive_event;
inline JuiceMAN g_juice_manager;