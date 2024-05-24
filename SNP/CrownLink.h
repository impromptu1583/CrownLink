#pragma once

#include "common.h"
#include "SNPNetwork.h"
#include "Output.h"
#include "Util/Types.h"
#include <vector>
#include <thread>
#include <chrono>

#include "signaling.h"

namespace clnk {

inline snp::NetworkInfo g_network_info{
	(char*)"CrownLink",
	'CLNK',
	(char*)"",

	// CAPS:
	{sizeof(CAPS), 0x20000003, snp::MAX_PACKET_SIZE, 16, 256, 1000, 50, 8, 2}
};

class CrownLink final : public snp::Network<NetAddress> {
public:
	CrownLink() = default;
	~CrownLink() override {
		m_is_running = false;
	}

	void initialize() override;
	void destroy() override;
	void requestAds() override;
	void receive() override {}; // unused in this connection type
	void sendAsyn(const NetAddress& to, Util::MemoryFrame packet) override;
	void startAdvertising(Util::MemoryFrame ad) override;
	void stopAdvertising() override;

	auto& juice_manager() { return m_juice_manager; }
	auto& signaling_socket() { return m_signaling_socket; }

private:
	void receive_signaling();
	void signal_handler(std::vector<SignalPacket>& incoming_packets);
	void error_handler(int n_bytes);
	void update_known_advertisers(const std::string& message);

private:
	JuiceManager m_juice_manager;
	SignalingSocket m_signaling_socket;

	std::jthread m_signaling_thread;
	std::vector<NetAddress> m_known_advertisers;
	Util::MemoryFrame m_ad_data;
    std::stop_source m_stop_source;

	bool m_is_advertising = false;
	bool m_is_running = true;
	bool m_client_id_set = false;
	NetAddress m_client_id;

	Logger m_logger{g_root_logger, "Juice"};
};

};

inline HANDLE g_receive_event;
inline ThQueue<GamePacket> g_receive_queue;
inline std::unique_ptr<clnk::CrownLink> g_crown_link;
