#pragma once

class SignalingSocket;

#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>
#include "Util/Exceptions.h"
#include <vector>

#ifndef __BLIZZARD_STORM_HEADER
struct SNETADDR {
	BYTE address[16];
};
#endif

class TWSAInitializerSig
{
public:
	TWSAInitializerSig();
	~TWSAInitializerSig();

public:
	static HANDLE completion_port;
};


class SignalingSocket
{
public:
	SignalingSocket();
	~SignalingSocket(); //destructor
	SNETADDR server;
	bool is_same_address(SNETADDR* a, SNETADDR* b);
	// state
private:
	SOCKET sockfd;
	struct addrinfo hints, * res, * p;
	int state;
	std::vector<std::string> split(std::string s);
	std::string _delimiter;

public:
	void init();
	void release() noexcept;
	void sendPacket(SNETADDR& dest, const char* msg);
	std::vector<std::string> receivePackets();
	void setBlockingMode(bool block);
	// need to do something so it's not blocking
	// some state args?
};

// send (to peer) (juice, not here)
// send (to server):
//	set advertising true/false
//	request list of advertisers
// recv (from peer) -> queue with critsec (juice, not here)
// recv (from server) -> handle list of ads, conn request
// end/close/destroy (send to server)

// touuuid[16bytes]message
// server uuid all 1s