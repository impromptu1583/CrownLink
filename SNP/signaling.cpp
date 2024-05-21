#include "signaling.h"

void to_json(json& j, const Signal_packet& p) {
	try {
		j = json{ {"peer_ID",p.peer_ID.b64()},
			{"message_type",p.message_type}, {"data",p.data} };
	}
	catch (const json::exception* e) {
		g_logger.error("Signal packet to_json error : {}",e->what());
	}

};
void from_json(const json& j, Signal_packet& p) {
	try {
		auto tempstr = j.at("peer_ID").get<std::string>();
		p.peer_ID = base64::from_base64(tempstr);
		j.at("message_type").get_to(p.message_type);
		j.at("data").get_to(p.data);
	}
	catch (const json::exception*e) {
		g_logger.error("Signal packet from_json error: {}. JSON dump: {}",e->what(),j.dump());
	}

};

SignalingSocket::SignalingSocket()
	: m_sockfd(NULL), server(), m_state(0), m_delimiter("-+"), m_host(), m_port(), m_initialized(false)
{
	//initialize();
}

SignalingSocket::~SignalingSocket()
{
	closesocket(m_sockfd);
	WSACleanup();
}
bool SignalingSocket::initialize()
{
	// init winsock
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;

	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		g_logger.error("WSAStartup failed with error {}", err);
	}

	struct addrinfo hints, * res, * p;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;


	if ((rv = getaddrinfo(
		snpconfig.load_or_default("server", "159.223.202.177").c_str(),
		snpconfig.load_or_default("port", "9988").c_str(),
		&hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		g_logger.error("getaddrinfo failed with error: ", rv);
		WSACleanup();
		return false;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((m_sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			g_logger.debug("client: socket {}", std::strerror(errno));
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(m_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(m_sockfd);
			perror("client: connect");
			g_logger.error("client: connect {}", std::strerror(errno));
			throw GeneralException("connection failed");
			continue;
		}

		break;
	}
	if (p == NULL) {
		g_logger.error("signaling client failed to connect");
		throw GeneralException("connection failed");
		return false;
	}

	freeaddrinfo(res);

	// server address: each byte is 11111111
	memset(&server, 255, sizeof(SNETADDR));

	m_initialized = true;
	set_blocking_mode(true);
	return true;
}

	void SignalingSocket::send_packet(const SNETADDR dest,
	const Signal_message_type msg_type, const std::string msg){
	//Signal_packet packet(dest, msg_type, msg);
	send_packet(Signal_packet(dest,msg_type,msg));
}
void SignalingSocket::send_packet(const Signal_packet& packet)
{
	if (!m_initialized) {
		g_logger.error("signal send_packet attempted but provider isn't initialized yet");
		return;
	}
	//std::string send_buffer;
	json j = packet;
	auto send_buffer = j.dump();
	send_buffer += m_delimiter;

	int n_bytes = send(m_sockfd, send_buffer.c_str(), send_buffer.size(), 0);
	if (n_bytes == -1) {
		g_logger.error("signaling send packet error: {}", std::strerror(errno));
	}
		
}

void SignalingSocket::split_into_packets(const std::string& s,std::vector<Signal_packet>& incoming_packets) {
	size_t pos_start = 0, pos_end, delim_len = m_delimiter.length();
	std::string segment;
	incoming_packets.clear();
	while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
		segment = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
			
		json j = json::parse(segment);
		Signal_packet p = j.template get<Signal_packet>();

		incoming_packets.push_back(p);
	}

	return;
}

void SignalingSocket::receive_packets(std::vector<Signal_packet>& incoming_packets){
	const unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string receive_buffer;
	g_logger.trace("receive_packets");
	// try to receive
	auto n_bytes = recv(m_sockfd, &buffer[0], buffer.size(), 0);
	if (n_bytes == SOCKET_ERROR) {
		m_state = WSAGetLastError();
		if (m_state == WSAEWOULDBLOCK || m_state == WSAECONNRESET) {
			// we're waiting for data or connection is reset
			// this is legacy of the old non-blocking code
			// we now block in a thread instead
			return;
		}
		if (m_state == WSAECONNRESET) {
			// connection reset by server
			// not sure if this should be a fatal or something
			g_logger.error("signaling socket receive error: connection reset by server");
		}
		else throw GeneralException("::recv failed");
	}
	else {
		buffer.resize(n_bytes);
		receive_buffer.append(buffer.cbegin(), buffer.cend());
		split_into_packets(receive_buffer,incoming_packets);
		return;
	}
}



void SignalingSocket::set_blocking_mode(bool block)
{
	u_long nonblock = !block;
	if (::ioctlsocket(m_sockfd, FIONBIO, &nonblock) == SOCKET_ERROR)
	{
		throw GeneralException("::ioctlsocket failed");
	}
}

void SignalingSocket::start_advertising(){
	send_packet(server, SIGNAL_START_ADVERTISING);
}

void SignalingSocket::stop_advertising(){
	send_packet(server, SIGNAL_STOP_ADVERTISING);
}
void SignalingSocket::request_advertisers(){
	send_packet(server, SIGNAL_REQUEST_ADVERTISERS);
}
