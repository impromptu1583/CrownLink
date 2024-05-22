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

struct AdFile {
	game game_info;
	char extra_bytes[32]{};
};

inline snp::NetworkInfo g_network_info{
	(char*)"CrownLink",
	'CLNK',
	(char*)"",

	// CAPS:
	{sizeof(CAPS), 0x20000003, snp::PACKET_SIZE, 16, 256, 1000, 50, 8, 2}
};

class JuiceP2P final : public snp::Network<SNETADDR> {
public:
	JuiceP2P() = default;
	~JuiceP2P() override = default;

	void initialize() override;
	void destroy() override;
	void requestAds() override;
	void receive() override {}; // unused in this connection type
	void sendAsyn(const SNETADDR& to, Util::MemoryFrame packet) override;
	void startAdvertising(Util::MemoryFrame ad) override;
	void stopAdvertising() override;

	void receive_signaling();
	void update_known_advertisers(const std::string& message);

private:
	std::jthread m_signaling_thread;
	std::vector<SNETADDR> m_known_advertisers;
	Util::MemoryFrame m_ad_data;
	bool m_is_advertising = false;
	bool m_is_running = true;
    std::stop_source m_stop_source;
};

};
