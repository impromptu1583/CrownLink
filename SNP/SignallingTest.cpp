#include "signaling.h"
#include <iostream>
#include <vector>
#include "JuiceManager.h"
#include <memory>
#include <chrono>
#include <thread>
#include "concurrentqueue.h"
#include "SNPNetwork.h"

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

	signaling::SignalingSocket signaling_socket;

	//signaling::Signal_packet p{ signaling_socket.server,
	//	signaling::SIGNAL_START_ADVERTISING, "" };

	//signaling_socket.send_packet(p);
	//signaling_socket.stop_advertising();
	signaling_socket.send_packet(signaling_socket.server,signaling::SIGNAL_START_ADVERTISING, "" );

	DWORD last_time = 0;
	DWORD now_time = GetTickCount();
	DWORD interval = 1000;

	std::vector<SNETADDR> m_known_advertisers;

	std::cout << "start advertising message\n";
	signaling_socket.start_advertising();
	signaling_socket.request_advertisers();


	//moodycamel::ConcurrentQueue<GamePacket> receive_queue;

	std::this_thread::sleep_for(std::chrono::milliseconds(1000));
	std::cout << "creating manager\n";
	JuiceMAN juice_manager(signaling_socket);
	std::cout << "done\n";
	std::string item;
	std::vector<signaling::Signal_packet> incoming_packets;
	int sendcounter = 0;
	std::string data_decoded;
	while (true) {
		incoming_packets = signaling_socket.receive_packets();
		for (auto packet : incoming_packets) {
			//if (packet.data.empty()) continue;
			std::cout << "data received:" << packet.data << "\n";
			std::cout << "data type:" << (int)packet.message_type << "\n";
			switch (packet.message_type)
			{
			case signaling::SIGNAL_START_ADVERTISING:
				std::cout << "start advertising confirmed\n";
				break;
			case signaling::SIGNAL_STOP_ADVERTISING:
				std::cout << "stop advertising confirmed\n";
				break;
			case signaling::SIGNAL_REQUEST_ADVERTISERS:
				data_decoded = base64::from_base64(packet.data);
				m_known_advertisers.clear();
				for (size_t i = 0; (i + 1) * sizeof(SNETADDR) <= data_decoded.size(); i++)
				{
					std::string peer_id = data_decoded.substr(i, sizeof(SNETADDR));
					m_known_advertisers.push_back(SNETADDR(peer_id));
					juice_manager.create_if_not_exist(peer_id);
				}
				std::cout << "got " << data_decoded.size() / sizeof(SNETADDR) << " peers\n";
				break;
			case signaling::SIGNAL_SOLICIT_ADS:
				if (true)
				{
					std::cout << "solicitaiton received\n";
					signaling_socket.send_packet(packet.peer_ID, signaling::SIGNAL_GAME_AD, "Your Ad Here");
				}
				break;
			case signaling::SIGNAL_GAME_AD:
				std::cout << "received game ad:" << packet.data << "\n";
				break;
			case signaling::SIGNAL_JUICE_LOCAL_DESCRIPTION:
				std::cout << "got local juice description";
				juice_manager.signal_handler(packet);
				break;
			case signaling::SIGNAL_JUICE_CANDIDATE:
				std::cout << "got candidate";
				juice_manager.signal_handler(packet);
				break;
			case signaling::SIGNAL_JUICE_DONE:
				std::cout << "juice done";
				juice_manager.signal_handler(packet);
				break;
			}
		}
		// do stuff here outside of receive loop
		
		now_time = GetTickCount();
		if (now_time - last_time > 1000) {
			last_time = now_time;
			sendcounter++;
			signaling_socket.request_advertisers();
			for (auto& advertiser : m_known_advertisers) {
				if (juice_manager.peer_status(advertiser) == JUICE_STATE_CONNECTED
					|| juice_manager.peer_status(advertiser) == JUICE_STATE_COMPLETED)
				{
					signaling_socket.send_packet(advertiser, signaling::SIGNAL_SOLICIT_ADS);
				}
			}
			std::string p2pmsg = std::format("Send number: {}", sendcounter);
			juice_manager.send_all(p2pmsg);

		}

		//while (receive_queue.try_dequeue(item)) {
		//	std::cout << "queue received from:" << item.substr(0,16) << " message: " << item.substr(16) << "\n";
		//}

		std::this_thread::sleep_for(std::chrono::milliseconds(1000));

	}




}


