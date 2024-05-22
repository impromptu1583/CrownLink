#pragma once
struct SignalPacket;
class SignalingSocket;

#include "common.h"

#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Util/Exceptions.h"
#include <vector>
#include <iostream>
#include <format>
#include "json.hpp"
#include "base64.hpp"
#include "SNPNetwork.h"

using json = nlohmann::json;

enum class SignalMessageType {
	SIGNAL_START_ADVERTISING = 1,
	SIGNAL_STOP_ADVERTISING,
	SIGNAL_REQUEST_ADVERTISERS,
	SIGNAL_SOLICIT_ADS,
	SIGNAL_GAME_AD,
	SIGNAL_JUICE_LOCAL_DESCRIPTION = 101,
	SIGNAL_JUICE_CANDIDATE,
	SIGNAL_JUICE_DONE,

	// debug
	SERVER_SET_ID = 254,
	SERVER_ECHO = 255
};

enum SocketState {
	SOCKET_STATE_UNINITIALIZED,
	SOCKET_STATE_INITIALIZED,
	SOCKET_STATE_CONNECTING,
	SOCKET_STATE_READY
};

inline std::string to_string(SocketState socket_state) {
	switch (socket_state) {
		case SocketState::SOCKET_STATE_UNINITIALIZED: return "Uninitialized";
		case SocketState::SOCKET_STATE_INITIALIZED: return "Initialized";
		case SocketState::SOCKET_STATE_CONNECTING: return "Connecting";
		case SocketState::SOCKET_STATE_READY: return "Ready";
	}
	return std::to_string((s32)socket_state);
}

struct SignalPacket {
	SNETADDR peer_id;
	SignalMessageType message_type;
	std::string data;

	SignalPacket() = default;

	SignalPacket(SNETADDR ID, SignalMessageType type, std::string d)
		: peer_id{ID}, message_type{type}, data{d} {}
	
	SignalPacket(std::string& packet_string) {
		memcpy_s((void*)&peer_id.address, sizeof(peer_id.address), packet_string.c_str(), sizeof(SNETADDR));
		message_type = SignalMessageType((int)packet_string.at(16) - 48);
		std::cout << (int)message_type << "msg type\n";
		data = packet_string.substr(sizeof(SNETADDR) + 1);
	}
};

class SignalingSocket {
public:
	SignalingSocket() = default;
	SignalingSocket(SignalingSocket&) = delete;
	SignalingSocket& operator=(SignalingSocket&) = delete;

	~SignalingSocket() {
		deinitialize();
	}

	bool initialize();
	void deinitialize();
	void send_packet(SNETADDR dest, SignalMessageType msg_type, const std::string& msg = "");
	void send_packet(const SignalPacket& packet);
	void receive_packets(std::vector<SignalPacket>& incoming_packets);
	void set_blocking_mode(bool block);
	void start_advertising();
	void stop_advertising();
	void request_advertisers();
	
private:
	void split_into_packets(const std::string& s, std::vector<SignalPacket>& incoming_packets);

private:
	SNETADDR m_server{};
	SocketState m_current_state = SocketState::SOCKET_STATE_UNINITIALIZED;
	SOCKET m_sockfd = NULL;
	int m_last_error;
	int m_state = 0;
	const std::string m_delimiter = "-+";
	std::string m_host;
	std::string m_port;
	bool m_initialized = false;
};

inline SignalingSocket g_signaling_socket;