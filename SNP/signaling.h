#pragma once

class SignalingSocket;

#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Util/Exceptions.h"
#include <vector>
#include "SNETADDR.h"
//#include "TWSAInitializer.h"

//class TWSAInitializerSig
//{
//public:
//	TWSAInitializerSig();
//	~TWSAInitializerSig();
//
//public:
//	static HANDLE completion_port;
//};


// message type solicit
// server will only deliver to advertisers
// advertisers will init p2p and (if) p2p is established
// send back game packet


enum Signal_message_type {
	SERVER_START_ADVERTISING = 1,
	SERVER_STOP_ADVERTISING,
	SERVER_REQUEST_ADVERTISERS,
	SERVER_SOLICIT,
	SERVER_GAME_AD,
	SERVER_SET_ID = 254,
	SERVER_ECHO = 255
};

class SignalingSocket
{
public:
	SignalingSocket();
	~SignalingSocket();
	void init();
	void release() noexcept;
	void send_packet(std::string dest, const std::string& msg);
	void send_packet(SNETADDR dest, const std::string& msg);
	std::vector<std::string> receive_packets();
	void set_blocking_mode(bool block);
	void start_advertising();
	void stop_advertising();
	void request_advertisers();
	SNETADDR server;

private:
	std::vector<std::string> split(const std::string& s);
	SOCKET m_sockfd;
	int m_state;
	const std::string m_delimiter;
};