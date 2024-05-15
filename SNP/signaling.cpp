#include "signaling.h"

namespace signaling
{

	void to_json(json& j, const Signal_packet& p) {
		std::string tempstr;
		tempstr.append((char*)p.peer_ID.address, sizeof(SNETADDR));
		j = json{ {"peer_ID",base64::to_base64(tempstr)},
			{"message_type",p.message_type}, {"data",p.data}};
	};
	void from_json(const json& j, Signal_packet& p) {
		std::string tempstr;
		j.at("peer_ID").get_to(tempstr);
		auto a = base64::from_base64(tempstr);
		p.peer_ID = base64::from_base64(tempstr);
		j.at("message_type").get_to(p.message_type);
		j.at("data").get_to(p.data);
	};

	Signal_packet test_json(Signal_packet& p) {
		json j = p;
		std::string out_str = j.dump();
		std::cout << "converted to:" << out_str << "\n";
		json n = json::parse(out_str);
		auto p2 = n.template get<Signal_packet>();
		return p2;
	}

	SignalingSocket::SignalingSocket()
		: m_sockfd(NULL), server(), m_state(0), m_delimiter("-+")
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

		if ((rv = getaddrinfo("159.223.202.177", "9988", &hints, &res)) != 0) {
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
		memset(&server, 255, sizeof(SNETADDR));

		set_blocking_mode(false);
	}

	SignalingSocket::~SignalingSocket()
	{
		release();
		WSACleanup();
	}

	void SignalingSocket::release() noexcept
	{
		::closesocket(m_sockfd);
	}

	void SignalingSocket::send_packet(SNETADDR dest,
		const Signal_message_type msg_type, std::string msg)
	{
		Signal_packet packet(dest, msg_type, msg);
		send_packet(packet);
	}
	void SignalingSocket::send_packet(const Signal_packet packet)
	{
		std::string send_buffer;

		json j = packet;
		send_buffer = j.dump();
		send_buffer += m_delimiter;

		int n_bytes = send(m_sockfd, send_buffer.c_str(), send_buffer.size(), 0);
		if (n_bytes == -1) perror("send error");
	}

	std::vector<Signal_packet> SignalingSocket::split_into_packets(const std::string& s) {
		size_t pos_start = 0, pos_end, delim_len = m_delimiter.length();
		std::string token;
		std::vector<Signal_packet> output;
		//size_t s_len = s.length();
		while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
			token = s.substr(pos_start, pos_end - pos_start);
			pos_start = pos_end + delim_len;
			
			json j = json::parse(token);
			Signal_packet p = j.template get<Signal_packet>();

			output.push_back(p);
		}

		return output;
	}

	std::vector<Signal_packet> SignalingSocket::receive_packets() {
		const unsigned int MAX_BUF_LENGTH = 4096;
		std::vector<char> buffer(MAX_BUF_LENGTH);
		std::string receive_buffer;
		std::vector<Signal_packet> output;
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
			receive_buffer.append(buffer.cbegin(), buffer.cend());
			output = split_into_packets(receive_buffer);
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
		send_packet(server, SIGNAL_START_ADVERTISING);
	}

	void SignalingSocket::stop_advertising()
	{
		send_packet(server, SIGNAL_STOP_ADVERTISING);
	}
	void SignalingSocket::request_advertisers()
	{
		send_packet(server, SIGNAL_REQUEST_ADVERTISERS);
	}
}
