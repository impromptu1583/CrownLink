#include "CrownLink.h"

#define BUFFER_SIZE 4096
constexpr auto ADDRESS_SIZE = 16;

CrownLink::CrownLink() {
	m_logger.info("Initializing, version {}", CL_VERSION);
	m_is_running = true;
	m_signaling_thread = std::jthread{&CrownLink::receive_signaling, this};
}

CrownLink::~CrownLink() {   
	m_logger.info("Shutting down");
	m_is_running = false;
	m_signaling_socket.echo(""); // wakes up m_signaling_thread so it can close
	m_signaling_socket.deinit();
	m_signaling_thread.join();
	m_logger.debug("Receive thread closed");
}

void CrownLink::request_advertisements() {
	m_logger.debug("Requesting lobbies");
	m_signaling_socket.request_advertisers();

	switch (m_signaling_socket.state()) {
		case SocketState::Ready: {
			snp::clear_status_ad();
		} break;
		default: {
			auto status_string = std::string{ "CrownLink Connecting" };
			m_ellipsis_counter = (m_ellipsis_counter + 1) % 4;
			for (u32 i = 0; i < m_ellipsis_counter; i++) {
				status_string += ".";
			}
			snp::set_status_ad(status_string);
		} break;
	}

	std::lock_guard lock{g_advertisement_mutex};
	for (const auto& advertiser : m_known_advertisers) {
		auto status = m_juice_manager.agent_state(advertiser);
		//if (status == JUICE_STATE_CONNECTED || status == JUICE_STATE_COMPLETED) {
			m_logger.trace("Requesting game state from {}", base64::to_base64(std::string{(char*)advertiser.bytes, sizeof(NetAddress)}));
			m_signaling_socket.send_packet(advertiser, SignalMessageType::SolicitAds);
		//}
	}
}

void CrownLink::send(const NetAddress& peer, void* data, size_t size) {
	m_juice_manager.send_p2p(std::string{(char*)peer.bytes, sizeof(NetAddress)}, data, size);
}

void CrownLink::receive_signaling() {
	m_signaling_socket.try_init();

	std::vector<SignalPacket> incoming_packets;
	while (m_is_running) {
		m_juice_manager.clear_inactive_agents();
		auto bytes = m_signaling_socket.receive_packets(incoming_packets);
		if (!m_is_running) return;
		if (bytes > 0) {
			handle_signal_packets(incoming_packets);
		} else {
			handle_winsock_error(bytes);
		}
	}
}

void CrownLink::handle_signal_packets(std::vector<SignalPacket>& packets) {
	for (const auto& packet : packets) {
		switch (packet.message_type) {
		case SignalMessageType::ServerSetID: {
			if (m_client_id_set) {
				m_signaling_socket.set_client_id(m_client_id.b64());
				if (m_is_advertising) {
					m_signaling_socket.start_advertising();
				}
			} else {
				m_client_id = base64::from_base64(packet.data);
				m_logger.info("received client ID from server: {}", m_client_id.b64());
				m_client_id_set = true;
			}
		} break;
		case SignalMessageType::RequestAdvertisers: {
			update_known_advertisers(packet.data);
		} break;
		case SignalMessageType::SolicitAds: {
			if (m_is_advertising) {
				m_logger.debug("received solicitation from {}, replying with our lobby info", packet.peer_address.b64());
				std::string send_buffer;
				send_buffer.append((char*)m_ad_data.begin(), m_ad_data.size());
				m_signaling_socket.send_packet(packet.peer_address, SignalMessageType::GameAd,
					base64::to_base64(send_buffer));
			}
		} break;
		case SignalMessageType::GameAd: {
			// -------------- PACKET: GAME STATS -------------------------------
			// Give the ad to storm
			m_logger.debug("received lobby info from {}", packet.peer_address.b64());
			auto decoded_data = base64::from_base64(packet.data);
			AdFile ad{};
			memcpy_s(&ad, sizeof(ad), decoded_data.c_str(), decoded_data.size());
			snp::pass_advertisement(packet.peer_address, Util::MemoryFrame::from(ad));

			NetAddress& netaddress = (NetAddress&)ad.game_info.saHost;
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
				netaddress.b64(),
				ad.game_info.dwTimer,
				ad.game_info.szGameName,
				ad.game_info.szGameStatString,
				ad.game_info.dwExtraBytes,
				ad.game_info.dwProduct,
				ad.game_info.dwVersion
			);
		} break;
		case SignalMessageType::SignalingPing:
		case SignalMessageType::JuiceTurnCredentials:
		case SignalMessageType::JuiceLocalDescription:
		case SignalMessageType::JuciceCandidate:
		case SignalMessageType::JuiceDone: {
			m_juice_manager.handle_signal_packet(packet);
		} break;
		}
	}
}

void CrownLink::handle_winsock_error(s32 error_code) {
	// Winsock error codes: https://learn.microsoft.com/en-us/windows/win32/winsock/windows-sockets-error-codes-2
	if (error_code == 0) {
		m_logger.error("Connection to server closed, attempting reconnect");
	} else {
		m_logger.error("Winsock error {} received, attempting reconnect", WSAGetLastError());
	}

	while (true) {
		m_signaling_socket.deinit();
		if (m_signaling_socket.try_init()) {
			break;
		}
		std::this_thread::sleep_for(std::chrono::seconds(1));
	}
}

void CrownLink::update_known_advertisers(const std::string& data) {
	std::lock_guard lock{g_advertisement_mutex};
	m_known_advertisers.clear();
	// SNETADDR in base64 encoding is always 24 characters
	Logger logger{m_logger, "update_known_advertisers"};
	logger.trace("Data received: {}", data);
	for (size_t i = 0; (i + 1) * 24 < data.size() + 1; i++) {
		try {
			const auto peer_str = base64::from_base64(data.substr(i*24, 24));
			logger.debug("Potential lobby owner received: {}", data.substr(i*24, 24));
			m_known_advertisers.push_back(NetAddress{peer_str});

			std::lock_guard lock{m_juice_manager.mutex()};
			m_juice_manager.ensure_agent(peer_str, lock);
		} catch (const std::exception &exc) {
			logger.error("Processing: {} error: {}", data.substr(i,24), exc.what());
		}
	}
}

void CrownLink::start_advertising(Util::MemoryFrame ad_data) {
	m_ad_data = ad_data;
	m_is_advertising = true;
	m_signaling_socket.start_advertising();
	m_logger.info("Started advertising lobby");
}

void CrownLink::stop_advertising() {
	m_is_advertising = false;
	m_signaling_socket.stop_advertising();
	m_logger.info("Stopped advertising lobby");
}
