#include "JuiceManager.h"

//----------------------------JuiceWrapper----------------------------//
// Used for individual P2P connections                                //
//--------------------------------------------------------------------//
JuiceWrapper::JuiceWrapper(const SNETADDR& ID, std::string init_message="")
	: p2p_state(JUICE_STATE_DISCONNECTED), m_ID(ID), m_config(),
	m_stun_server("stun.l.google.com"),	m_stun_server_port(19302), m_sdp()
{
	m_ID_b64 = m_ID.b64();
	memset(&m_config, 0, sizeof(m_config));
	m_config.stun_server_host = m_stun_server.c_str();
	m_config.stun_server_port = m_stun_server_port;
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
	signaling_socket.send_packet(m_ID, SIGNAL_JUICE_LOCAL_DESCRIPTION,m_sdp);
	g_logger.trace("[P2P Agent][{}] Init - local SDP {}", m_ID_b64, m_sdp);
	juice_gather_candidates(m_agent);
}
JuiceWrapper::~JuiceWrapper()
{
}
void JuiceWrapper::signal_handler(const Signal_packet packet)
{

	switch (packet.message_type)
	{
	case SIGNAL_JUICE_LOCAL_DESCRIPTION:
		juice_set_remote_description(m_agent, packet.data.c_str());
		g_logger.trace("[P2P Agent][{}] received remote description:\n{}", m_ID_b64, packet.data);
		break;
	case SIGNAL_JUICE_CANDIDATE:
		juice_add_remote_candidate(m_agent, packet.data.c_str());
		g_logger.trace("[P2P Agent][{}] received remote candidate {}", m_ID_b64, packet.data);
		break;
	case SIGNAL_JUICE_DONE:
		juice_set_remote_gathering_done(m_agent);
		g_logger.trace("[P2P Agent][{}] remote gathering done", m_ID_b64);
		break;
	}
}

void JuiceWrapper::send_message(const std::string& msg)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		if (g_logger.log_level() == log_level_trace) {
			g_logger.trace("[P2P Agent][{}] sending message {}", m_ID_b64, msg);
		}
		juice_send(m_agent, msg.c_str(), msg.size());
	} // todo error handling
	else if(p2p_state == JUICE_STATE_FAILED){
		g_logger.error("[P2P Agent][{}] trying to send message but P2P connection failed");
	}
}
void JuiceWrapper::send_message(const char* begin, const size_t size)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		if (g_logger.log_level() == log_level_trace) {
			g_logger.trace("[P2P Agent][{}] sending message {}", m_ID_b64, std::string(begin,size));
		}
		juice_send(m_agent, begin, size);
	} // todo error handling
	else if (p2p_state == JUICE_STATE_FAILED) {
		g_logger.error("[P2P Agent][{}] trying to send message but P2P connection failed");
	}
}
void JuiceWrapper::send_message(Util::MemoryFrame frame)
{
	send_message((char*)frame.begin(), frame.size());
	//if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
	//	if (g_logger.log_level() == log_level_trace) {
	//		g_logger.trace("[P2P Agent][{}] sending message {}", m_ID_b64, std::string((char*)frame.begin(),frame.size()));
	//	}
	//	juice_send(m_agent, (char*)frame.begin(), frame.size());
	//} // todo error handling
	//else if (p2p_state == JUICE_STATE_FAILED) {
	//	g_logger.error("[P2P Agent][{}] trying to send message but P2P connection failed");
	//}
}

void JuiceWrapper::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr)
{
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->p2p_state = state;
	if (state == JUICE_STATE_CONNECTED) {
		g_logger.info("[P2P Agent][{}] Initially connected", parent->m_ID_b64);

	}
	else if (state == JUICE_STATE_COMPLETED) {
		g_logger.info("[P2P Agent][{}] Connecttion negotiation finished", parent->m_ID_b64);
		char local[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		char remote[JUICE_MAX_CANDIDATE_SDP_STRING_LEN];
		juice_get_selected_candidates(agent, local, JUICE_MAX_CANDIDATE_SDP_STRING_LEN, remote, JUICE_MAX_CANDIDATE_SDP_STRING_LEN);
		if (std::string(local).find("typ relay") != std::string::npos) {
			g_logger.warn("[P2P Agent][{}] local connection is relayed, performance may be affected", parent->m_ID_b64);
		}
		if (std::string(remote).find("typ relay") != std::string::npos) {
			g_logger.warn("[P2P Agent][{}] remote connection is relayed, performance may be affected", parent->m_ID_b64);
		}
		if (g_logger.log_level() == log_level_trace) {
			g_logger.trace("[P2P Agent][{}] Final candidates were local: {} remote: {}", parent->m_ID_b64, local, remote);
		}
	}
	else if (state == JUICE_STATE_FAILED) {
		g_logger.error("[P2P Agent][{}] Could not connect, gave up", parent->m_ID_b64);
	}
}
void JuiceWrapper::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr){
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	signaling_socket.send_packet(parent->m_ID, SIGNAL_JUICE_CANDIDATE, sdp);

}
void JuiceWrapper::on_gathering_done(juice_agent_t* agent, void* user_ptr){
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	signaling_socket.send_packet(parent->m_ID, SIGNAL_JUICE_DONE);
}
void JuiceWrapper::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr){
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	receive_queue.emplace(GamePacket{ parent->m_ID,data,size });
	SetEvent(receiveEvent);
	if (g_logger.log_level() == log_level_trace) {
		g_logger.trace("[P2P Agent][{}] received: {}", parent->m_ID_b64, std::string(data,size));
	}
}




//------------------------------JuiceMAN------------------------------//
// Maps individual P2P connections (JuiceWrapper)                     //
//--------------------------------------------------------------------//
void JuiceMAN::create_if_not_exist(const std::string& ID)
{
	if (!m_agents.count(ID))
	{
		m_agents.insert(std::make_pair(ID, new JuiceWrapper(ID)));
	}
}

void JuiceMAN::send_p2p(const std::string& ID, const std::string& msg)
{
	//todo error handling
	create_if_not_exist(ID);
	m_agents[ID]->send_message(msg);
}
void JuiceMAN::send_p2p(const std::string& ID, Util::MemoryFrame frame)
{
	//todo error handling
	create_if_not_exist(ID);
	m_agents[ID]->send_message(frame);
}


void JuiceMAN::signal_handler(const Signal_packet packet)
{
	auto peerstr = std::string((char*)packet.peer_ID.address, sizeof(SNETADDR));
	g_logger.trace("[P2P Manager] received message for {}: {}",peerstr,packet.data);
	if (!m_agents.count(peerstr)) {
		m_agents.insert(std::make_pair(peerstr, new JuiceWrapper(peerstr, packet.data)));
	}
	m_agents[peerstr]->signal_handler(packet);
}


void JuiceMAN::send_all(const std::string& msg)
{
	for (const auto& kv : m_agents) {
		//std::cout << "\x1b[2K" << "peer status: " << kv.second->p2p_state;
		std::cout << "sending message peer " << (char*)kv.second->m_ID.address << " with status:" << kv.second->p2p_state << "\n";
		kv.second->send_message(msg);
	}
}
void JuiceMAN::send_all(const char* begin, const size_t size)
{
	for (const auto& kv : m_agents) {
		//std::cout << "\x1b[2K" << "peer status: " << kv.second->p2p_state;
		std::cout << "sending message peer " << (char*)kv.second->m_ID.address << " with status:" << kv.second->p2p_state << "\n";
		kv.second->send_message(begin,size);
	}
}
void JuiceMAN::send_all(Util::MemoryFrame frame)
{
	for (const auto& kv : m_agents) {
		//std::cout << "\x1b[2K" << "peer status: " << kv.second->p2p_state;
		kv.second->send_message(frame);
		std::cout << "sending message peer " << (char*)kv.second->m_ID.address << " with status:" << kv.second->p2p_state << "\n";
	}
}

juice_state JuiceMAN::peer_status(SNETADDR peer_ID)
{
	// yes, it's ugly but I'm still working on changing out strings for SNETADDR
	auto id = std::string((char*)peer_ID.address, sizeof(SNETADDR));
	if (m_agents.count(id)) {
		return m_agents[id]->p2p_state;
	}
	return juice_state(JUICE_STATE_DISCONNECTED);
}
