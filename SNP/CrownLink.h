#pragma once

#include "Common.h"
#include "Output.h"
#include "Util/Types.h"
#include <vector>
#include <thread>
#include <chrono>

#include "signaling.h"

inline snp::NetworkInfo g_network_info{
	(char*)"CrownLink",
	'CLNK',
	(char*)"",

	// CAPS:
	{sizeof(CAPS), 0x20000003, snp::MAX_PACKET_SIZE, 16, 256, 1000, 50, 8, 2}
};

class CrownLink {
public:
	CrownLink();
	~CrownLink();

	CrownLink(const CrownLink&) = delete;
	CrownLink& operator=(const CrownLink&) = delete;

	void request_advertisements();
	void send(const NetAddress& to, void* data, size_t size);
	void start_advertising(Util::MemoryFrame ad_data);
	void stop_advertising();

	auto& receive_queue() { return m_receive_queue; }
	auto& juice_manager() { return m_juice_manager; }
	auto& signaling_socket() { return m_signaling_socket; }

private:
	void receive_signaling();
	void handle_signal_packets(std::vector<SignalPacket>& packets);
	void handle_winsock_error(s32 error_code);
	void update_known_advertisers(const std::string& message);

private:
	ThQueue<GamePacket> m_receive_queue;
	JuiceManager m_juice_manager;
	SignalingSocket m_signaling_socket;

	std::jthread m_signaling_thread;
	std::vector<NetAddress> m_known_advertisers;
	Util::MemoryFrame m_ad_data;

	bool m_is_advertising = false;
	bool m_is_running = true;
	bool m_client_id_set = false;
	NetAddress m_client_id;

	Logger m_logger{Logger::root(), "Juice"};
};

inline HANDLE g_receive_event;
inline std::unique_ptr<CrownLink> g_crown_link;
inline std::mutex g_advertisement_mutex;
