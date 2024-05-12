#pragma once

class SignalingSocket;

#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Util/Exceptions.h"
#include <vector>
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



enum Signal_message_type {
	SERVER_START_ADVERTISING = 1,
	SERVER_STOP_ADVERTISING,
	SERVER_REQUEST_ADVERTISERS,
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
	std::vector<std::string> receive_packets();
	void set_blocking_mode(bool block);
	std::string server;

private:
	std::vector<std::string> split(const std::string& s);
	SOCKET m_sockfd;
	int m_state;
	const std::string m_delimiter;
};