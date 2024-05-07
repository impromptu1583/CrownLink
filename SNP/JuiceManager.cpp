#include "JuiceManager.h"



JuiceMAN::JuiceMAN()
	: _base_config(), _stun_server(), _stun_server_port(), _agents()
{
	
}
JuiceMAN::~JuiceMAN()
{
	release();
}
void JuiceMAN::init() {
	release();
	_stun_server = "stun.l.google.com";
	_stun_server_port = 19302;

	//-------------generate base config for all agents-------------//
	memset(&_base_config, 0, sizeof(_base_config));
	_base_config.stun_server_host = _stun_server.c_str();
	_base_config.stun_server_port = _stun_server_port;
	//----callbacks----//
	_base_config.cb_state_changed = on_state_changed;
	_base_config.cb_candidate = on_candidate;
	_base_config.cb_gathering_done = on_gathering_done;
	_base_config.cb_recv = on_recv;
	//----Use single thread---//
	_base_config.concurrency_mode = JUICE_CONCURRENCY_MODE_POLL;
	//----User Pointer Unused---//
	//_base_config.user_ptr = NULL;
}

void JuiceMAN::create_agent(SNETADDR dest_id) {
	juice_config_t this_config = _base_config;
	this_config.user_ptr = &dest_id; //TODO will this get destroyed?
	_agents[dest_id.address] = juice_create(&this_config);
	//----Get Local Description----//
	char sdp[JUICE_MAX_SDP_STRING_LEN];
	juice_get_local_description(_agents[dest_id.address], sdp, JUICE_MAX_SDP_STRING_LEN);
	//----Send Local Desc. to Peer ----//
	std::string sdpstr = "";
	sdpstr += 1; // 1 = description, 2 = candidate, 3 = gathering done
	sdpstr.append(sdp, sizeof(sdp));
	_sigsock.sendPacket(dest_id, sdpstr);
	juice_gather_candidates(_agents[dest_id.address]);
}

void JuiceMAN::release() noexcept {

}

void JuiceMAN::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr) {
}
void JuiceMAN::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr) {
	std::string sdpstr = "";
	sdpstr += 2; // 1 = description, 2 = candidate, 3 = gathering done
	sdpstr.append(sdp, sizeof(sdp));
	SNETADDR dest;
	memcpy((dest.address), user_ptr, sizeof(SNETADDR));
	_sigsock.sendPacket(dest, sdpstr);
}
void JuiceMAN::on_gathering_done(juice_agent_t* agent, void* user_ptr) {
	std::string sdpstr = "";
	sdpstr += 3; // 1 = description, 2 = candidate, 3 = gathering done
	SNETADDR dest;
	memcpy((dest.address), user_ptr, sizeof(SNETADDR));
	_sigsock.sendPacket(dest, sdpstr);
}
void JuiceMAN::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr) {
}