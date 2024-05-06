#include "signaling.h"

SignalingSocket session;

int main(int argc, char* argv[])
{

	session.init();
	session.setBlockingMode(false);

	std::string msg;
	msg += reinterpret_cast<const char*>(session.server.address);
	msg += 1;

	session.sendPacket(session.server, msg);

	std::string buf;
	while (true) {
		session.receivePacket(&buf);
	}


	/*
	recvsvr();
	msg = reinterpret_cast<const char*>(server.address);
	msg += 3;
	sendsvr(server, msg);
	recvsvr();

	recvsvr();
	closesocket(sockfd);*/


}


