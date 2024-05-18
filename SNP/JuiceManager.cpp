#include "JuiceManager.h"

//----------------------------JuiceWrapper----------------------------//
// Used for individual P2P connections                                //
//--------------------------------------------------------------------//
JuiceWrapper::JuiceWrapper(const SNETADDR& ID, signaling::SignalingSocket& sig_sock,
	std::string init_message="")
	: p2p_state(JUICE_STATE_DISCONNECTED), m_ID(ID), m_config(),
	m_stun_server("stun.l.google.com"),	m_stun_server_port(19302), m_signaling_socket(sig_sock), m_sdp()
{

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
	//send_signaling_message(m_sdp, juice_signal_local_description);
	m_signaling_socket.send_packet(m_ID, signaling::SIGNAL_JUICE_LOCAL_DESCRIPTION,m_sdp);
	juice_gather_candidates(m_agent);
}
JuiceWrapper::~JuiceWrapper()
{
}
void JuiceWrapper::signal_handler(const signaling::Signal_packet packet)
{

	switch (packet.message_type)
	{
	case signaling::SIGNAL_JUICE_LOCAL_DESCRIPTION:
		juice_set_remote_description(m_agent, packet.data.c_str());
		break;
	case signaling::SIGNAL_JUICE_CANDIDATE:
		juice_add_remote_candidate(m_agent, packet.data.c_str());
		break;
	case signaling::SIGNAL_JUICE_DONE:
		juice_set_remote_gathering_done(m_agent);
		break;
	}
}

void JuiceWrapper::send_message(const std::string& msg)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		juice_send(m_agent, msg.c_str(), msg.size());
	} // todo error handling
}
void JuiceWrapper::send_message(const char* begin, const size_t size)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		juice_send(m_agent, begin, size);
	} // todo error handling
}
void JuiceWrapper::send_message(Util::MemoryFrame frame)
{
	if (p2p_state == JUICE_STATE_CONNECTED || p2p_state == JUICE_STATE_COMPLETED) {
		std::string buf;
		buf.append((char*)frame.begin(), frame.size());

		juice_send(m_agent, (char*)frame.begin(), frame.size());
	} // todo error handling
}

void JuiceWrapper::on_state_changed(juice_agent_t* agent, juice_state_t state, void* user_ptr)
{
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->p2p_state = state;
	if (state == JUICE_STATE_CONNECTED) {
		std::cout << "Successfully connected P2P\n";
	}
	else if (state == JUICE_STATE_COMPLETED) {
		std::cout << "Juice Completed\n";
	}
}
void JuiceWrapper::on_candidate(juice_agent_t* agent, const char* sdp, void* user_ptr)
{
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->m_signaling_socket.send_packet(parent->m_ID, signaling::SIGNAL_JUICE_CANDIDATE, sdp);

}
void JuiceWrapper::on_gathering_done(juice_agent_t* agent, void* user_ptr)
{
	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;
	parent->m_signaling_socket.send_packet(parent->m_ID, signaling::SIGNAL_JUICE_DONE);
}
void JuiceWrapper::on_recv(juice_agent_t* agent, const char* data, size_t size, void* user_ptr)
{

	JuiceWrapper* parent = (JuiceWrapper*)user_ptr;

	//GamePacket gamePacket;
	//memcpy(gamePacket.data, data, size);
	//gamePacket.packetSize = size;
	//gamePacket.sender = parent->m_ID;
	//gamePacket.timeStamp = GetTickCount();
	//receive_queue.push(GamePacket(gamePacket));
	receive_queue.emplace(GamePacket{ parent->m_ID,size,data });
	SetEvent(receiveEvent);
}




//------------------------------JuiceMAN------------------------------//
// Maps individual P2P connections (JuiceWrapper)                     //
//--------------------------------------------------------------------//
void JuiceMAN::create_if_not_exist(const std::string& ID)
{
	if (!m_agents.count(ID))
	{
		m_agents.insert(std::make_pair(ID, new JuiceWrapper(ID, m_signaling_socket)));
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


//void JuiceMAN::signal_handler(const std::string& source_ID, const std::string& msg)
//{
//	std::cout << "received signal: " << msg << "\n";
//	if (!m_agents.count(source_ID)) {
//		//m_agents[source_ID] = std::unique_ptr<JuiceWrapper>(new JuiceWrapper(source_ID, m_signaling_socket,msg));
//		m_agents.insert(std::make_pair(source_ID, new JuiceWrapper(source_ID, m_signaling_socket, p_receive_queue,msg)));
//	}
//	m_agents[source_ID]->signal_handler(msg);
//}
void JuiceMAN::signal_handler(const signaling::Signal_packet packet)
{
	std::string peerstr;
	peerstr.append((char*)packet.peer_ID.address, sizeof(SNETADDR));
	if (!m_agents.count(peerstr)) {
		//m_agents[source_ID] = std::unique_ptr<JuiceWrapper>(new JuiceWrapper(source_ID, m_signaling_socket,msg));
		m_agents.insert(std::make_pair(peerstr, new JuiceWrapper(peerstr, m_signaling_socket, packet.data)));
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
	std::string id((char*)peer_ID.address, sizeof(SNETADDR));
	if (m_agents.count(id)) {
		return m_agents[id]->p2p_state;
	}
	return juice_state(JUICE_STATE_DISCONNECTED);
}
