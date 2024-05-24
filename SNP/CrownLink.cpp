#include "CrownLink.h"

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

namespace clnk {

void CrownLink::initialize() {
	m_logger.info("Initializing, version {}", CL_VERSION);
	g_signaling_socket.initialize();
	m_signaling_thread = std::jthread{&CrownLink::receive_signaling, this};
}

void CrownLink::destroy() {   
	m_logger.info("Shutting down");
	m_is_running = false;
	g_signaling_socket.echo(""); // wakes up m_signaling_thread so it can close
	m_signaling_thread.join();
	m_logger.debug("Receive thread closed");

}

void CrownLink::requestAds() {
	m_logger.debug("Requesting lobbies");
	g_signaling_socket.request_advertisers();
	for (const auto& advertiser : m_known_advertisers) {
		auto status = g_juice_manager.peer_status(advertiser);
		if (status == JUICE_STATE_CONNECTED || status == JUICE_STATE_COMPLETED) {
			m_logger.trace("Requesting game state from {}", base64::to_base64(std::string((char*)advertiser.address, sizeof(SNetAddr))));
			g_signaling_socket.send_packet(advertiser, SignalMessageType::SolicitAds);
		}
	}
}

void CrownLink::sendAsyn(const SNetAddr& peer_ID, Util::MemoryFrame packet){
	g_juice_manager.send_p2p(std::string((char*)peer_ID.address, sizeof(SNetAddr)), packet.begin(), packet.size());
}

void CrownLink::receive_signaling() {
	std::vector<SignalPacket> incoming_packets;
	while (true) {
		auto n_bytes = g_signaling_socket.receive_packets(incoming_packets);
		if (!m_is_running) return;
		if (n_bytes > 0) {
			signal_handler(incoming_packets);
		} else {
			error_handler(n_bytes);
		}
	}
}

void CrownLink::signal_handler(std::vector<SignalPacket>& incoming_packets) {
	for (const auto& packet : incoming_packets) {
		switch (packet.message_type) {
			case SignalMessageType::ServerSetID:
			{
				if (m_client_id_set) {
					g_signaling_socket.set_client_id(m_client_id.b64());
					if (m_is_advertising) {
						g_signaling_socket.start_advertising();
					}
				} else {
					m_client_id = base64::from_base64(packet.data);
					m_logger.info("received client ID from server: {}", m_client_id.b64());
					m_client_id_set = true;
				}
			} break;
			case SignalMessageType::StartAdvertising: {
			} break;
			case SignalMessageType::StopAdvertising: {
			} break;
			case SignalMessageType::RequestAdvertisers: {
				update_known_advertisers(packet.data);
			} break;
			case SignalMessageType::SolicitAds: {
				if (m_is_advertising) {
					m_logger.debug("received solicitation from {}, replying with our lobby info", packet.peer_id.b64());
					std::string send_buffer;
					send_buffer.append((char*)m_ad_data.begin(), m_ad_data.size());
					g_signaling_socket.send_packet(packet.peer_id, SignalMessageType::GameAd,
						base64::to_base64(send_buffer));
				}
			} break;
			case SignalMessageType::GameAd: {
				// -------------- PACKET: GAME STATS -------------------------------
				// Give the ad to storm
				m_logger.debug("received lobby info from {}", packet.peer_id.b64());
				auto decoded_data = base64::from_base64(packet.data);
				AdFile ad{};
				memcpy_s(&ad, sizeof(ad), decoded_data.c_str(), decoded_data.size());
				snp::passAdvertisement(packet.peer_id, Util::MemoryFrame::from(ad));

				m_logger.debug("Game Info Received:\n"
					"  dwIndex: {}\n"
					"  dwGameState: {}\n"
					"  saHost: {}\n"
					"  dwTimer: {}\n"
					"  szGameName[128]: {}\n"
					"  szGameStatString[128]: {}\n"
					"  dwExtraBytes: {}\n"
					"  dwProduct: {}\n"
					"  dwVersion: {}\n",
					ad.game_info.dwIndex,
					ad.game_info.dwGameState,
					ad.game_info.saHost.b64(),
					ad.game_info.dwTimer,
					ad.game_info.szGameName,
					ad.game_info.szGameStatString,
					ad.game_info.dwExtraBytes,
					ad.game_info.dwProduct,
					ad.game_info.dwVersion
				);
			} break;
			case SignalMessageType::JuiceLocalDescription:
			case SignalMessageType::JuciceCandidate:
			case SignalMessageType::JuiceDone: {
				g_juice_manager.handle_signal_packet(packet);
			} break;
		}
	}
}
void CrownLink::error_handler(int n_bytes) {
	// winsock error codes: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2

	if (n_bytes == 0) {
		m_logger.error("connection to server closed, attempting reconnect");
	} else {
		auto ws_error = WSAGetLastError();
		m_logger.error("winsock error {} received, attempting reconnect", ws_error);
	}
	do {
		g_signaling_socket.deinitialize();
		std::this_thread::sleep_for(std::chrono::seconds(1));
	} while (!g_signaling_socket.initialize());
	
}



void CrownLink::update_known_advertisers(const std::string& data) {
	m_known_advertisers.clear();
	// SNETADDR in base64 encoding is always 24 characters
	Logger logger{m_logger, "update_known_advertisers"};
	logger.trace("data received: {}", data);
	for (size_t i = 0; (i + 1) * 24 < data.size() + 1; i++) {
		try {
			auto peer_str = base64::from_base64(data.substr(i*24, 24));
			logger.debug("potential lobby owner received: {}", data.substr(i*24, 24));
			m_known_advertisers.push_back(SNetAddr{peer_str});
			g_juice_manager.ensure_agent(peer_str);
		} catch (const std::exception &exc) {
			logger.error("processing: {} error: {}", data.substr(i,24), exc.what());

		}
	}
}

void CrownLink::startAdvertising(Util::MemoryFrame ad) {
	m_ad_data = ad;
	m_is_advertising = true;
	g_signaling_socket.start_advertising();
	m_logger.info("started advertising lobby");
}

void CrownLink::stopAdvertising() {
	m_is_advertising = false;
	g_signaling_socket.stop_advertising();
	m_logger.info("stopped advertising lobby");
}

};
