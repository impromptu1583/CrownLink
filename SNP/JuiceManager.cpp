#include "JuiceManager.h"

JuiceWrapper::JuiceWrapper(std::string ID, SignalingSocket* sig_sock, std::string init_message="")
	: p2p_state(JUICE_STATE_DISCONNECTED), m_ID(), m_config(), sm_stun_server("stun.l.google.com"), sm_stun_server_port(19302), m_signalling_socket(sig_sock), m_sdp()
{
	memcpy((m_ID.address), ID.c_str(), sizeof(SNETADDR));
	memset(&m_config, 0, sizeof(m_config));
	m_config.stun_server_host = sm_stun_server.c_str();
	m_config.stun_server_port = sm_stun_server_port;
	//----callbacks----//
	m_config.cb_state_changed = on_state_changed;
	m_config.cb_candidate = on_candidate;
	m_config.cb_gathering_done = on_gathering_done;
	m_config.cb_recv = on_recv;
	m_config.user_ptr = this;
	m_agent = juice_create(&m_config);
	if (!init_message.empty()) {
		signal_handler(init_message);
	}
	juice_get_local_description(m_agent, m_sdp, JUICE_MAX_SDP_STRING_LEN);
	send_signaling_message(m_sdp, juice_signal_local_description);
	juice_gather_candidates(m_agent);
}
JuiceWrapper::~JuiceWrapper()
{
}

void JuiceWrapper::send_signaling_message(char* msg, Juice_signal msgtype)
{
	std::string send_buffer;
	send_buffer += msgtype;
	send_buffer += msg;
	m_signalling_socket->sendPacket(m_ID, send_buffer.c_str());
}

void JuiceWrapper::signal_handler(std::string message)
{
	std::string message_without_type = message.substr(1);
	switch (message[0])
	{
	case juice_signal_local_description:
		juice_set_remote_description(m_agent, message_without_type.c_str());
		break;
	case juice_signal_candidate:
		juice_add_remote_candidate(m_agent, message_without_type.c_str());
		break;
	case juice_signal_gathering_done:
		juice_set_remote_gathering_done(m_agent);
		break;
	}
}

void JuiceWrapper::send_message(std::string msg)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		//juice_send(m_agent, msg.c_str(), sizeof(msg.c_str()));
		std::string testmsg = "test message";
		std::cout << "size:" << strlen(testmsg.c_str());
		juice_send(m_agent, testmsg.c_str(), strlen(testmsg.c_str()));
	} // todo error handling
}

void JuiceWrapper::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr)
{
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->p2p_state = state;

}
void JuiceWrapper::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr)
{
	std::string send_buffer;
	send_buffer += juice_signal_candidate;
	send_buffer += sdp;
	std::cout << "sending candidate: " << send_buffer << "\n";
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->m_signalling_socket->sendPacket(parent->m_ID, send_buffer.c_str());

}
void JuiceWrapper::on_gathering_done(juice_agent_t* agent, void* user_ptr)
{
	std::string send_buffer;
	send_buffer += juice_signal_gathering_done;
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->m_signalling_socket->sendPacket(parent->m_ID, send_buffer.c_str());
}
void JuiceWrapper::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr)
{
	// add to queue
	std::string recv_buffer;
	recv_buffer.append(data, size);
	std::cout << "received p2p:" << recv_buffer << "\n";

}



void JuiceMAN::send_p2p(std::string ID, char* message)
{
	//todo error handling
	if (m_agents.count(ID)) {
		m_agents[ID]->send_message(message);
	}
	else {
		m_agents[ID] = std::unique_ptr<JuiceWrapper>(new JuiceWrapper(ID, m_signaling_socket));
	}
	// what to do with message? I guess we just drop if there's no connection...
}

void JuiceMAN::signal_handler(std::string source_ID, std::string msg)
{
	// check if we have an agent already
	// if not, create one
	// pass message to agent
	std::cout << "received signal: " << msg << "\n";
	if (!m_agents.count(source_ID)) {
		m_agents[source_ID] = std::unique_ptr<JuiceWrapper>(new JuiceWrapper(source_ID, m_signaling_socket,msg));
	}
	m_agents[source_ID]->signal_handler(msg);
}


void JuiceMAN::send_all(std::string msg)
{
	for (const auto& kv : m_agents) {
		std::cout << "\x1b[2K" << "peer status: " << kv.second->p2p_state;
		kv.second->send_message(msg);
	}
}
