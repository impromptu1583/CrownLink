#pragma once
struct Signal_packet;
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

enum Signal_message_type {
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

struct Signal_packet {
	Signal_packet() = default;
	Signal_packet(SNETADDR ID, Signal_message_type type, std::string d)
		:peer_ID(ID), message_type(type), data(d)
	{};
	Signal_packet(std::string& packet_string)
	{
		memcpy((void*)&(peer_ID.address), packet_string.c_str(), sizeof(SNETADDR));
		message_type = Signal_message_type((int)packet_string.at(16) - 48);
		std::cout << (int)message_type << "msg type\n";
		data = packet_string.substr(sizeof(SNETADDR) + 1);
	};
	SNETADDR peer_ID;
	Signal_message_type message_type;
	std::string data;
};

class SignalingSocket
{

public:
	SignalingSocket();
	~SignalingSocket();
	//void release() noexcept;
	bool initialize();
	void send_packet(const SNETADDR dest, const Signal_message_type msg_type, const std::string msg = "");
	void send_packet(const Signal_packet&);
	void receive_packets(std::vector<Signal_packet>& incoming_packets);
	void set_blocking_mode(bool block);
	void start_advertising();
	void stop_advertising();
	void request_advertisers();
	SNETADDR server;

private:
	void split_into_packets(const std::string& s, std::vector<Signal_packet>& incoming_packets);
	SOCKET m_sockfd;
	int m_state;
	const std::string m_delimiter;
	std::string m_host;
	std::string m_port;
	bool m_initialized;
};


inline SignalingSocket signaling_socket;