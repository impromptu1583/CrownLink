#include "signaling.h"

#define BUFFER_SIZE 4096

TWSAInitializerSig::TWSAInitializerSig()
{
    WSADATA WsaDat;
    if (WSAStartup(MAKEWORD(2, 2), &WsaDat) != 0)
        throw GeneralException("WSA initialization failed");
    // TWSAInitializer::completion_port = CreateIoCompletionPort(NULL, NULL, NULL, 0);
}

TWSAInitializerSig::~TWSAInitializerSig()
{
    WSACleanup();
}

TWSAInitializerSig _init_wsa;

// constructors

SignalingSocket::SignalingSocket()
    : sockfd(NULL), hints(),res(),p(),server(),state(0),_delimiter("-+")
{
}

SignalingSocket::~SignalingSocket()
{
    release();
}

void SignalingSocket::init()
{
    release();

	//int sockfd;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo("localhost", "9999", &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("client: socket");
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(sockfd);
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

	//u_long mode = 1; //enable non-blocking
	//ioctlsocket(sockfd, FIONBIO, &mode);
	// need to add error checking

	// server address is 1111111111111111
	memset(&server, 255, sizeof server);
}

void SignalingSocket::release() noexcept
{
	::closesocket(sockfd);
}

void SignalingSocket::sendPacket(SNETADDR& dest, std::string msg) {
	msg.insert(0, (char*)(dest.address), sizeof(dest.address));
	msg.append(_delimiter);
	int n_bytes = send(sockfd, msg.c_str(), msg.length(), 0);
	if (n_bytes == -1) perror("send error");
}

std::vector<std::string> SignalingSocket::split(std::string s) {
	size_t pos_start = 0, pos_end, delim_len = _delimiter.length();
	std::string token;
	std::vector<std::string> res;
	size_t s_len = s.length();
	while ((pos_end = s.find(_delimiter, pos_start)) != std::string::npos) {
		token = s.substr(pos_start, pos_end - pos_start);
		s_len = token.length();
		pos_start = pos_end + delim_len;
		res.push_back(token);
	}

	//res.push_back(s.substr(pos_start));
	return res;
}

bool SignalingSocket::is_same_address(SNETADDR *a, SNETADDR *b) {
	return (0 == memcmp(a, b, sizeof(SNETADDR)));
}

std::vector<std::string> SignalingSocket::receivePackets() {
	const unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string rcv;
	std::vector<std::string> output;
	int n_bytes;

	// try to receive
	n_bytes = recv(sockfd, &buffer[0], buffer.size(), 0);
	if (n_bytes == SOCKET_ERROR) {
		state = WSAGetLastError();
		if (state == WSAEWOULDBLOCK || state == WSAECONNRESET) {
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

	//std::cout << n_bytes << " bytes received" << std::endl;
	//buff[n_bytes] = '\0';
	//std::cout << buff << std::endl;

	//int i;
	//for (i = 0; i < n_bytes; i++)
	//{
	//	if ((i - 1) % 16 == 0) printf("\n");
	//	if (i > 0) printf(":");
	//	printf("%02hhX", rcv[i]);
	//}
	//printf("\n");

	//memcpy(&buf, &rcv, sizeof(rcv));
	//return 1;
	//return buff;


void SignalingSocket::setBlockingMode(bool block)
{
	u_long nonblock = !block;
	if (::ioctlsocket(sockfd, FIONBIO, &nonblock) == SOCKET_ERROR)
	{
		throw GeneralException("::ioctlsocket failed");
	}
}