#pragma once

#include <juice.h>
#include <string>
#include <map>
#include "signaling.h"
#include "Util/MemoryFrame.h"



class JuiceMAN
{
public:
	JuiceMAN(SignalingSocket& sigsock);
	~JuiceMAN();

private:
	juice_config_t _base_config;
	std::string _stun_server;
	int _stun_server_port;
	SignalingSocket _sigsock;
	std::map<BYTE*,juice_agent_t*> _agents;
	void _create_agent(SNETADDR dest_id);
	static void _on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr);
	static void _on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr);
	static void _on_gathering_done(juice_agent_t* agent, void* user_ptr);
	static void _on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr);

public:
	void init();
	void release() noexcept;
	void signalling_received(std::string msg);
	void sendPacket(const SNETADDR &target, Util::MemoryFrame data);
	Util::MemoryFrame receivePacket(SNETADDR& from, Util::MemoryFrame data);
};

