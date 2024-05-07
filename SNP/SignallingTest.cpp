#include "signaling.h"
#include <iostream>
#include <vector>
#include "JuiceManager.h"

SignalingSocket session;

void printhex(const char* hex) {
	int i;
	for (i = 0; i < sizeof(hex)*2; i++)
	{
		if ((i - 1) % 16 == 0) printf("\n");
		if (i > 0) printf(":");
		printf("%02hhX", hex[i]);
	}
	printf("\n");
}


int main(int argc, char* argv[])
{

	session.init();
	session.setBlockingMode(false);

	std::string msg;
	msg += 1;

	session.sendPacket(session.server, msg);

	std::string msg2;
	msg2 += 3;
	session.sendPacket(session.server, msg2);

	// juice


	//std::string buf;
	//int res;
	std::vector<std::string> output;
	while (true) {
		output = session.receivePackets();
		for (auto i : output) {
			if (i.empty()) continue;
			// we've received a packet... what do we do?
			SNETADDR from_address;
			memcpy((from_address.address), i.substr(0, 16).c_str(), sizeof(SNETADDR));
			std::string rcv_msg = i.substr(16);
			if (session.is_same_address(&from_address, &session.server)) {
				// message came from server
				std::cout << "new message from server:";
				std::cout << rcv_msg << std::endl;
				switch (rcv_msg.at(0)) {
				case 1:
					std::cout << "confirmed advertising\n";
					break;
				case 2:
					std::cout << "confirmed stopped advertising\n";
					break;
				case 3:
					std::cout << "list of advertisers returned\n";
					// do something here
					for (int i = 0; (i + 1) * sizeof(SNETADDR) + 1 <= rcv_msg.size(); i++) {
						std::string chunk = rcv_msg.substr(i+1, sizeof(SNETADDR));
						printhex(chunk.c_str());
						SNETADDR peer;
						memcpy((peer.address), chunk.c_str(), sizeof(SNETADDR));
						std::string peermsg = "Hi buddy!";
						session.sendPacket(peer, peermsg);
					}
					break;
				}
			}
			else {
				// from a peer??
				std::cout << "got a message from a peer: " << rcv_msg << "\n";
			}

			//std::cout << i << std::endl;
			//printhex(i.c_str());

		}
		//res = session.receivePacket(&buf);
		//if (res) {
			//std::cout << buf << std::endl;
			//int i;
			//for (i = 0; i < sizeof(buf); i++)
			//{
			//	if ((i - 1) % 16 == 0) printf("\n");
			//	if (i > 0) printf(":");
			//	printf("%02hhX", buf[i]);
			//}
			//printf("\n");
			//res = 0;
		//}

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


