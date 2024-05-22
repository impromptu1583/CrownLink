#include "signaling.h"

void to_json(json& out_json, const SignalPacket& packet) {
	try {
		out_json = json{
			{"peer_ID",packet.peer_id.b64()},
			{"message_type",packet.message_type},
			{"data",packet.data},
		};
	} catch (const json::exception& e) {
		g_root_logger.error("Signal packet to_json error : {}", e.what());
	}
};

void from_json(const json& json_, SignalPacket& out_packet) {
	try {
		auto tempstr = json_.at("peer_ID").get<std::string>();
		out_packet.peer_id = base64::from_base64(tempstr);
		json_.at("message_type").get_to(out_packet.message_type);
		json_.at("data").get_to(out_packet.data);
	} catch (const json::exception& ex) {
		g_root_logger.error("Signal packet from_json error: {}. JSON dump: {}", ex.what(), json_.dump());
	}
};

bool SignalingSocket::initialize() {
	g_root_logger.info("connecting to matchmaking server");
	m_current_state = SocketState::Connecting;
	struct addrinfo hints, * res, * p;
	int rv;
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo(
		g_snp_config.load_or_default("server", "159.223.202.177").c_str(),
		g_snp_config.load_or_default("port", "9988").c_str(),
		&hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		g_root_logger.error("getaddrinfo failed with error: ", rv);
		WSACleanup();
		return false;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		if ((m_sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			g_root_logger.debug("client: socket failed with error: {}", std::strerror(errno));
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(m_sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(m_sockfd);
			g_root_logger.error("client: couldn't connect to server: {}", std::strerror(errno));
			throw GeneralException("server connection failed");
			continue;
		}

		break;
	}
	if (p == NULL) {
		g_root_logger.error("signaling client failed to connect");
		throw GeneralException("server connection failed");
		return false;
	}

	freeaddrinfo(res);

	// server address: each byte is 11111111
	memset(&m_server, 255, sizeof(SNetAddr));

	m_initialized = true;
	set_blocking_mode(true);
	g_root_logger.info("successfully connected to matchmaking server");
	m_current_state = SocketState::Ready;
	return true;
}

void SignalingSocket::deinitialize() {
	closesocket(m_sockfd);
}

void SignalingSocket::send_packet(SNetAddr dest, SignalMessageType msg_type, const std::string& msg) {
	send_packet(SignalPacket(dest,msg_type,msg));
}

void SignalingSocket::send_packet(const SignalPacket& packet) {
	if (m_current_state != SocketState::Ready) {
		g_root_logger.error("signal send_packet attempted but provider is not ready. State: {}", as_string(m_current_state));
		return;
	}

	json json_ = packet;
	auto send_buffer = json_.dump();
	send_buffer += m_delimiter;
	g_root_logger.debug("Sending to server, buffer size: {}, contents: {}", send_buffer.size(), send_buffer);

	int bytes = send(m_sockfd, send_buffer.c_str(), send_buffer.size(), 0);
	if (bytes == -1) {
		g_root_logger.error("signaling send packet error: {}", std::strerror(errno));
	}
}

void SignalingSocket::split_into_packets(const std::string& s,std::vector<SignalPacket>& incoming_packets) {
	size_t pos_start = 0;
	size_t pos_end = 0;
	size_t delim_len = m_delimiter.length();
	std::string segment;

	incoming_packets.clear();
	while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
		segment = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
			
		json json_ = json::parse(segment);
		incoming_packets.push_back(json_.template get<SignalPacket>());
	}

	return;
}

void SignalingSocket::receive_packets(std::vector<SignalPacket>& incoming_packets){
	constexpr unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string receive_buffer;
	g_root_logger.trace("receive_packets");
	// try to receive
	auto n_bytes = recv(m_sockfd, &buffer[0], buffer.size(), 0);
	if (n_bytes == SOCKET_ERROR) {
		m_last_error = WSAGetLastError();
		g_root_logger.debug("receive error: {}", m_last_error);
		if (m_last_error == WSAEWOULDBLOCK) {
			// we're waiting for data or connection is reset
			// this is legacy of the old non-blocking code
			// we now block in a thread instead
			return;
		}
		if (m_last_error == WSAECONNRESET) {
			// connection reset by server
			// not sure if this should be a fatal or something
			g_root_logger.error("signaling socket receive error: connection reset by server, attempting reconnect");
			deinitialize();
			std::this_thread::sleep_for(std::chrono::seconds(1));
			initialize();

		}
		else throw GeneralException("::recv failed");
	}
	if (n_bytes <1) {
		// 0 = connection closed by remote end
		// -1 = winsock error
		// anything more = we actually got data
		g_root_logger.error("server connection closed, attempting reconnect");
		deinitialize();
		std::this_thread::sleep_for(std::chrono::seconds(1));
		initialize();
	} else {
		buffer.resize(n_bytes);
		receive_buffer.append(buffer.begin(), buffer.end());
		split_into_packets(receive_buffer, incoming_packets);
		g_root_logger.trace("received {}", n_bytes);
		return;
	}
}

void SignalingSocket::set_blocking_mode(bool block) {
	u_long nonblock = !block;
	if (::ioctlsocket(m_sockfd, FIONBIO, &nonblock) == SOCKET_ERROR) {
		throw GeneralException("::ioctlsocket failed");
	}
}

void SignalingSocket::start_advertising(){
	send_packet(m_server, SignalMessageType::StartAdvertising);
}

void SignalingSocket::stop_advertising(){
	send_packet(m_server, SignalMessageType::StopAdvertising);
}

void SignalingSocket::request_advertisers(){
	send_packet(m_server, SignalMessageType::RequestAdvertisers);
}
