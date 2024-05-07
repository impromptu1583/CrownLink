#pragma once

#include <juice.h>
#include <string>
#include <map>
#include "signaling.h"
#include "Util/MemoryFrame.h"
#include "ThQueue/ThQueue.h" // Thanks Veeq7!



class JuiceMAN
{
public:
	JuiceMAN();
	~JuiceMAN();

private:
	juice_config_t _base_config;
	std::string _stun_server;
	int _stun_server_port;
	static SignalingSocket _sigsock;
	std::map<BYTE*,juice_agent_t*> _agents;
	void create_agent(SNETADDR dest_id);
	static void on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

public:
	void init();
	void release() noexcept;
	void signalling_received(std::string msg);
	void sendPacket(const SNETADDR &target, Util::MemoryFrame data);
	Util::MemoryFrame receivePacket(SNETADDR& from, Util::MemoryFrame data);
};

