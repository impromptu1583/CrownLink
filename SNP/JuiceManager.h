#pragma once

#include <juice.h>
#include <string>
#include <unordered_map>
#include "signaling.h"
#include "Util/MemoryFrame.h"
#include "iostream"

#include "ThQueue/ThQueue.h" // Thanks Veeq7!

enum Juice_signal {
	juice_signal_local_description = 1,
	juice_signal_candidate = 2,
	juice_signal_gathering_done = 3
};

class JuiceWrapper
{
public:
	JuiceWrapper(const std::string& ID, SignalingSocket& sig_sock, std::string init_message);
	~JuiceWrapper();
	void signal_handler(const std::string& msg);
	void send_message(const std::string& msg);
	juice_state p2p_state;

private:
	void send_signaling_message(char* msg, Juice_signal msgtype);
	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

	std::string m_ID;
	juice_config_t m_config;
	const std::string m_stun_server = "stun.l.google.com";
	const int m_stun_server_port = 19302;
	SignalingSocket m_signalling_socket;
	juice_agent_t *m_agent;
	char m_sdp[JUICE_MAX_SDP_STRING_LEN];

};


class JuiceMAN
{
public:
	JuiceMAN(SignalingSocket& sig_sock)
		: m_agents(),m_signaling_socket(sig_sock)
	{};
	~JuiceMAN() {};

	void create_if_not_exist(const std::string& ID);
	void send_p2p(const std::string& ID, const std::string& msg);
	void signal_handler(const std::string& source_ID, const std::string& msg);
	void send_all(const std::string&);
private:
	std::unordered_map<std::string,std::unique_ptr<JuiceWrapper>> m_agents;
	SignalingSocket m_signaling_socket;
};

