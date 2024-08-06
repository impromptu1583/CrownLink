#pragma once

#include "Common.h"
#include <vector>
#include <thread>
#include <chrono>

#include "JuiceManager.h"

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
	void start_advertising(AdFile ad_data);
	void send_advertisement();
	void stop_advertising();
	void pass_advertisements(const std::vector<AdFile>& advertisements);

	auto& advertising() { return m_is_advertising; };
	auto& receive_queue() { return m_receive_queue; }
	auto& juice_manager() { return m_juice_manager; }
	auto& crowserve() { return m_crowserve; }

	void set_id(const NetAddress& id) {m_client_id = id; m_client_id_set = true;}
	void set_token(const NetAddress& token) {m_reconnect_token = token;}

	void set_mode(const CrownLinkMode& v) { m_cl_version = v; }
	CrownLinkMode mode() const { return m_cl_version; }

private:
	void init_listener();

private:
	//ThQueue<GamePacket> m_receive_queue;
	moodycamel::ConcurrentQueue<GamePacket> m_receive_queue;
	CrowServe::Socket m_crowserve;

	JuiceManager m_juice_manager;
	AdFile m_ad_data;

	bool m_is_advertising = false;
	bool m_is_running = true;
	bool m_client_id_set = false;
	NetAddress m_client_id;
	NetAddress m_reconnect_token;

	u32 m_ellipsis_counter = 3;
	CrownLinkMode m_cl_version = CrownLinkMode::CLNK;
};

inline HANDLE g_receive_event;
inline std::unique_ptr<CrownLink> g_crown_link;
inline std::mutex g_advertisement_mutex;
