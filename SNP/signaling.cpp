#include "signaling.h"

SignalingSocket::SignalingSocket()
    : m_sockfd(NULL), server(),m_state(0),m_delimiter("-+")
{

	// init winsock
	WSADATA wsaData;
	WORD wVersionRequested;
	int err;

	wVersionRequested = MAKEWORD(2, 2);
	err = WSAStartup(wVersionRequested, &wsaData);
	

	release();

	struct addrinfo hints, * res, * p;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;
	//159.223.202.177

	if ((rv = getaddrinfo("159.223.202.177", "9999", &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((m_sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("client: socket");
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(m_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(m_sockfd);
			perror("client: connect");
			throw GeneralException("connection failed");
			continue;
		}

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		throw GeneralException("connection failed");
		return;
	}

	freeaddrinfo(res);

	// server address: each byte is 11111111
	//memset(&server, 255, sizeof(server));
	//char addr[16];
	//memset(&addr, 255, sizeof(addr));
	//server.append(addr,16);

	memset(&server, 255, sizeof(SNETADDR));

	set_blocking_mode(false);
}

SignalingSocket::~SignalingSocket()
{
    release();
	WSACleanup();
}

void SignalingSocket::init()
{

}

void SignalingSocket::release() noexcept
{
	::closesocket(m_sockfd);
}


void SignalingSocket::send_packet(std::string dest, const std::string& msg) {
	dest += msg;
	dest += m_delimiter;

	int n_bytes = send(m_sockfd, dest.c_str(), dest.size(), 0);
	if (n_bytes == -1) perror("send error");
}
void SignalingSocket::send_packet(SNETADDR dest, const std::string& msg) {
	std::string send_buffer;
	send_buffer.append((char*)dest.address, sizeof(SNETADDR));
	send_buffer += msg;
	send_buffer += m_delimiter;

	int n_bytes = send(m_sockfd, send_buffer.c_str(), send_buffer.size(), 0);
	if (n_bytes == -1) perror("send error");
}

std::vector<std::string> SignalingSocket::split(const std::string& s) {
	size_t pos_start = 0, pos_end, delim_len = m_delimiter.length();
	std::string token;
	std::vector<std::string> res;
	size_t s_len = s.length();
	while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		s_len = token.length();
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	//res.push_back(s.substr(pos_start));
	return res;
}

std::vector<std::string> SignalingSocket::receive_packets() {
	const unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string rcv;
	std::vector<std::string> output;
	int n_bytes;

	// try to receive
	n_bytes = recv(m_sockfd, &buffer[0], buffer.size(), 0);
	if (n_bytes == SOCKET_ERROR) {
		m_state = WSAGetLastError();
		if (m_state == WSAEWOULDBLOCK || m_state == WSAECONNRESET) {
			// we're waiting for data or connection is reset
			return output; //will be empty
		}
		else throw GeneralException("::recv failed");
	}
	else {
		buffer.resize(n_bytes);
		rcv.append(buffer.cbegin(), buffer.cend());
		output = split(rcv);
		return output;
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

void SignalingSocket::start_advertising()
{
	std::string message;
	message += Signal_message_type(SERVER_START_ADVERTISING);
	send_packet(server, message);
}

void SignalingSocket::stop_advertising()
{
	std::string message;
	message += Signal_message_type(SERVER_STOP_ADVERTISING);
	send_packet(server, message);
}
void SignalingSocket::request_advertisers()
{
	std::string message;
	message += Signal_message_type(SERVER_REQUEST_ADVERTISERS);
	send_packet(server, message);
}