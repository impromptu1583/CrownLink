#pragma once

class SignalingSocket;

#include <string.h>
#define WIN32_LEAN_AND_MEAN
#include <winsock2.h>
#include <ws2tcpip.h>

class SignalingSocket
{
public:
	SignalingSocket();
	~SignalingSocket(); //destructor

	// state
private:
	SOCKET _s;
	// prob also address info....

public:
	void init();
	void release() noexcept;
	void bind(int port);
	void sendPacket(); //need to add vars
	const char* receivePacket(); // need to add vars
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