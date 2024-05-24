#include "signaling.h"

void to_json(Json& out_json, const SignalPacket& packet) {
	try {
		out_json = Json{
			{"peer_id", packet.peer_id.b64()},
			{"message_type", packet.message_type},
			{"data", packet.data},
		};
	} catch (const Json::exception& e) {
		g_root_logger.error("Signal packet to_json error : {}", e.what());
	}
};

void from_json(const Json& json_, SignalPacket& out_packet) {
	try {
		auto tempstr = json_.at("peer_ID").get<std::string>();
		out_packet.peer_id = base64::from_base64(tempstr);
		json_.at("message_type").get_to(out_packet.message_type);
		json_.at("data").get_to(out_packet.data);
	} catch (const Json::exception& ex) {
		g_root_logger.error("Signal packet from_json error: {}. JSON dump: {}", ex.what(), json_.dump());
	}
};

bool SignalingSocket::initialize() {
	m_logger.info("connecting to matchmaking server");
	m_current_state = SocketState::Connecting;

	addrinfo hints = {}; 
	addrinfo* result = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if (const auto error = getaddrinfo(g_config.server.c_str(), std::to_string(g_config.port).c_str(), &hints, &result)) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(error));
		m_logger.error("getaddrinfo failed with error: {}", error);
		WSACleanup();
		return false;
	}

	for (auto info = result; info; info = info->ai_next) {
		if ((m_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
			m_logger.debug("client: socket failed with error: {}", std::strerror(errno));
			throw GeneralException("socket failed");
			continue;
		}

		if (connect(m_socket, info->ai_addr, info->ai_addrlen) == -1) {
			closesocket(m_socket);
			m_logger.error("client: couldn't connect to server: {}", std::strerror(errno));
			throw GeneralException("server connection failed");
			continue;
		}

		break;
	}
	if (result == NULL) {
		m_logger.error("signaling client failed to connect");
		throw GeneralException("server connection failed");
		return false;
	}

	freeaddrinfo(result);

	// server address: each byte is 11111111
	memset(&m_server, 255, sizeof(SNetAddr));

	set_blocking_mode(true);
	m_logger.info("successfully connected to matchmaking server");
	m_current_state = SocketState::Ready;
	return true;
}

void SignalingSocket::deinitialize() {
	if (m_socket) {
		closesocket(m_socket);
		m_socket = 0;
	}
}

void SignalingSocket::send_packet(SNetAddr dest, SignalMessageType msg_type, const std::string& msg) {
	send_packet(SignalPacket{dest, msg_type, msg});
}

void SignalingSocket::send_packet(const SignalPacket& packet) {
	if (m_current_state != SocketState::Ready) {
		m_logger.error("signal send_packet attempted but provider is not ready. State: {}", as_string(m_current_state));
		return;
	}

	Json json = packet;
	auto send_buffer = json.dump();
	send_buffer += m_delimiter;
	m_logger.debug("Sending to server, buffer size: {}, contents: {}", send_buffer.size(), send_buffer);

	int bytes = send(m_socket, send_buffer.c_str(), send_buffer.size(), 0);
	if (bytes == -1) {
		m_logger.error("signaling send packet error: {}", std::strerror(errno));
	}
}

void SignalingSocket::split_into_packets(const std::string& s,std::vector<SignalPacket>& incoming_packets) {
	size_t pos_start = 0;
	size_t pos_end = 0;
	size_t delim_len = m_delimiter.length();

	incoming_packets.clear();
	while ((pos_end = s.find(m_delimiter, pos_start)) != std::string::npos) {
		const auto segment = s.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
			
		Json json = Json::parse(segment);
		incoming_packets.push_back(json.template get<SignalPacket>());
	}
}

void SignalingSocket::receive_packets(std::vector<SignalPacket>& incoming_packets, int& n_bytes, int& ws_error) {
	constexpr unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string receive_buffer;
	n_bytes = recv(m_socket, &buffer[0], buffer.size(), 0);
	if(n_bytes) {
		buffer.resize(n_bytes);
		receive_buffer.append(buffer.begin(), buffer.end());
		split_into_packets(receive_buffer, incoming_packets);
		m_logger.trace("received {}", n_bytes);
	}
}

void SignalingSocket::set_blocking_mode(bool block) {
	u_long nonblock = !block;
	if (::ioctlsocket(m_socket, FIONBIO, &nonblock) == SOCKET_ERROR) {
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

void SignalingSocket::echo(std::string data) {
	send_packet(m_server, SignalMessageType::ServerEcho, data);
}

void SignalingSocket::set_client_id(std::string id) {
	send_packet(m_server, SignalMessageType::ServerSetID, id);
}