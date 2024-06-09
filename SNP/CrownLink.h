#pragma once

#include "Common.h"
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

enum class CrownLinkMode {
	CLNK, // standard version
	DBCL  // double brain cells version
};

inline std::string to_string(CrownLinkMode value) {
	switch (value) {
		EnumStringCase(CrownLinkMode::CLNK);
		EnumStringCase(CrownLinkMode::DBCL);
	}
	return std::to_string((s32) value);
}

class CrownLink {
public:
	CrownLink();
	~CrownLink();

	CrownLink(const CrownLink&) = delete;
	CrownLink& operator=(const CrownLink&) = delete;

	void request_advertisements();
	void send(const NetAddress& to, void* data, size_t size);
	void start_advertising(AdFile ad_data);
	void stop_advertising();

	auto& receive_queue() { return m_receive_queue; }
	auto& juice_manager() { return m_juice_manager; }
	auto& signaling_socket() { return m_signaling_socket; }

	void set_mode(const CrownLinkMode& v) { m_cl_version = v; }
	CrownLinkMode mode() const { return m_cl_version; }

private:
	void receive_signaling();
	void handle_signal_packets(std::vector<SignalPacket>& packets);
	void handle_winsock_error(s32 error_code);
	void update_known_advertisers(const std::string& message);

private:
	//ThQueue<GamePacket> m_receive_queue;
	moodycamel::ConcurrentQueue<GamePacket> m_receive_queue;
	JuiceManager m_juice_manager;
	SignalingSocket m_signaling_socket;

	std::jthread m_signaling_thread;
	std::vector<NetAddress> m_known_advertisers;
	AdFile m_ad_data;

	bool m_is_advertising = false;
	bool m_is_running = true;
	bool m_client_id_set = false;
	NetAddress m_client_id;

	u32 m_ellipsis_counter = 3;
	CrownLinkMode m_cl_version = CrownLinkMode::CLNK;
};

inline HANDLE g_receive_event;
inline std::unique_ptr<CrownLink> g_crown_link;
inline std::mutex g_advertisement_mutex;
