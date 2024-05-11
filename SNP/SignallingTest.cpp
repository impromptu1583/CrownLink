#include "signaling.h"
#include <iostream>
#include <vector>
#include "JuiceManager.h"
#include <memory>
#include <chrono>
#include <thread>

constexpr auto ADDRESS_SIZE = 16;

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

	SignalingSocket signaling_socket;

	std::string msg;
	msg += Signal_message_type(SERVER_START_ADVERTISING);
	std::cout << "start advertising message";
	
	signaling_socket.send_packet(signaling_socket.server, msg);

	std::string msg2;
	msg2 += Signal_message_type(SERVER_REQUEST_ADVERTISERS);
	std::cout << "sending  msg";
	signaling_socket.send_packet(signaling_socket.server, msg2);

	// juice
	//auto p_recv_queue = std::make_shared<ThQueue<std::string>>();
	JuiceMAN juice_manager(signaling_socket);

	std::string peer_id;

	//std::string buf;
	//int res;
	std::vector<std::string> output;
	while (true) {
		output = signaling_socket.receive_packets();
		for (auto i : output) {
			if (i.empty()) continue;
			std::string from_address = i.substr(0, 16);
			std::string rcv_msg = i.substr(16);
			if (signaling_socket.server.compare(from_address)==0){
				// message came from server
				std::cout << "new message from server:";
				std::cout << rcv_msg << std::endl;
				switch (rcv_msg.at(0)) {
				case Signal_message_type(SERVER_START_ADVERTISING):
					std::cout << "confirmed advertising\n";
					break;
				case Signal_message_type(SERVER_STOP_ADVERTISING):
					std::cout << "confirmed stopped advertising\n";
					break;
				case Signal_message_type(SERVER_REQUEST_ADVERTISERS):
					std::cout << "list of advertisers returned\n";
					// do something here
					for (size_t i = 0; (i + 1) * ADDRESS_SIZE + 1 <= rcv_msg.size(); i++) {
						std::string chunk = rcv_msg.substr(i+1, ADDRESS_SIZE);
						if (i == 0) {
							peer_id += chunk;
						}
						printhex(chunk.c_str());
						//char* testmsg = "hi";
						//juice_manager.send_p2p(peer_id, testmsg);
						juice_manager.create_if_not_exist(peer_id);
					}

					
					break;
				}
			}
			else {
				// from a peer??
				std::cout << "got a message from a peer: " << rcv_msg << "\n";
				juice_manager.signal_handler(i.substr(0, 16), rcv_msg);
			}


		}
		// do stuff here outside of receive loop
		std::string p2pmsg = "sentent";
		juice_manager.send_all(p2pmsg);
		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

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


