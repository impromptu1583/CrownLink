#include "signaling.h"

void to_json(Json& out_json, const SignalPacket& packet) {
	try {
		out_json = {
			{"peer_id", packet.peer_address.b64()},
			{"message_type", packet.message_type},
			{"data", packet.data},
		};
	} catch (const Json::exception& e) {
		spdlog::dump_backtrace();
		spdlog::error("Signal packet to_json error : {}", e.what());
	}
};

void from_json(const Json& json, SignalPacket& out_packet) {
	try {
		auto tempstr = json.at("peer_id").get<std::string>();
		out_packet.peer_address = base64::from_base64(tempstr);
		json.at("message_type").get_to(out_packet.message_type);
		json.at("data").get_to(out_packet.data);
	} catch (const Json::exception& ex) {
		spdlog::dump_backtrace();
		spdlog::error("Signal packet from_json error: {}. JSON dump: {}", ex.what(), json.dump());
	}
};

bool SignalingSocket::try_init() {
	spdlog::info("Connecting to matchmaking server");
	m_current_state = SocketState::Connecting;

	addrinfo hints = {}; 
	addrinfo* result = nullptr;
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	const auto& snp_config = SnpConfig::instance();
	if (const auto error = getaddrinfo(snp_config.server.c_str(), std::to_string(snp_config.port).c_str(), &hints, &result)) {
		spdlog::dump_backtrace();
		spdlog::error("getaddrinfo failed with error {}: {}", error, gai_strerror(error));
		return false;
	}

	for (auto info = result; info; info = info->ai_next) {
		if ((m_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
			spdlog::error("Client: Socket failed with error: {}", std::strerror(errno));
			continue;
		}

		if (connect(m_socket, info->ai_addr, info->ai_addrlen) == -1) {
			closesocket(m_socket);
			spdlog::dump_backtrace();
			spdlog::error("Client: Couldn't connect to server: {}", std::strerror(errno));
			continue;
		}

		break;
	}
	if (!result) {
		spdlog::dump_backtrace();
		spdlog::error("Signaling client failed to connect");
		return false;
	}

	freeaddrinfo(result);

	// Server address: each byte is 11111111
	memset(&m_server, 255, sizeof(NetAddress));

	spdlog::info("Successfully connected to matchmaking server");
	m_current_state = SocketState::Ready;
	return true;
}

void SignalingSocket::deinit() {
	if (m_socket) {
		closesocket(m_socket);
		m_socket = 0;
	}
}

void SignalingSocket::send_packet(NetAddress dest, SignalMessageType msg_type, const std::string& msg) {
	send_packet(SignalPacket{dest, msg_type, msg});
}

void SignalingSocket::send_packet(const SignalPacket& packet) {
	if (m_current_state != SocketState::Ready) {
		spdlog::dump_backtrace();
		spdlog::error("Signal send_packet attempted but provider is not ready. State: {}", as_string(m_current_state));
		return;
	}

	std::lock_guard lock{m_mutex};
	Json json = packet;
	auto send_buffer = json.dump();
	send_buffer += Delimiter;
	spdlog::trace("Sending to server, buffer size: {}, contents: {}", send_buffer.size(), send_buffer);

	int bytes = send(m_socket, send_buffer.c_str(), send_buffer.size(), 0);
	if (bytes == -1) {
		spdlog::dump_backtrace();
		spdlog::error("Signaling send packet error: {}", std::strerror(errno));
	}
}

void SignalingSocket::split_into_packets(const std::string& data, std::vector<SignalPacket>& incoming_packets) {
	size_t pos_start = 0;
	size_t pos_end = 0;
	size_t delim_len = Delimiter.length();

	incoming_packets.clear();
	while ((pos_end = data.find(Delimiter, pos_start)) != std::string::npos) {
		const auto segment = data.substr(pos_start, pos_end - pos_start);
		pos_start = pos_end + delim_len;
		try {
			Json json = Json::parse(segment);
			incoming_packets.push_back(json.template get<SignalPacket>());
		}
		catch (std::exception& e) {
			spdlog::dump_backtrace();
			spdlog::error("could not parse JSON \"{}\", exc: {}", segment, e.what());
		};
	}
}

s32 SignalingSocket::receive_packets(std::vector<SignalPacket>& incoming_packets) {
	static constexpr unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);
	std::string receive_buffer;
	auto bytes = recv(m_socket, &buffer[0], buffer.size(), 0);
	if (bytes > 0) {
		spdlog::trace("Received {}", bytes);
		buffer.resize(bytes);
		receive_buffer.append(buffer.begin(), buffer.end());
		split_into_packets(receive_buffer, incoming_packets);
	}
	return bytes;
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

void SignalingSocket::echo(const std::string& data) {
	send_packet(m_server, SignalMessageType::ServerEcho, data);
}

void SignalingSocket::set_client_id(const std::string& id) {
	send_packet(m_server, SignalMessageType::ServerSetID, id);
}

SocketState SignalingSocket::state() {
	return m_current_state;
}
