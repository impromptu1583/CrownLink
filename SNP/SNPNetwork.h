#pragma once

#include "Util/MemoryFrame.h"
#include "Util/Types.h"
#include "Storm/storm.h"

namespace snp {

constexpr auto MAX_PACKET_SIZE = 500;

struct NetworkInfo {
	char* pszName;
	DWORD dwIdentifier;
	char* pszDescription;
	CAPS  caps;
};

using SOCKADDR = ::SOCKADDR;

void passAdvertisement(const NetAddress& host, Util::MemoryFrame ad);
void removeAdvertisement(const NetAddress& host);
void passPacket(GamePacket& parket);

template<typename PeerId>
class Network {
public:
	virtual ~Network() = default;

	NetAddress makeBin(const PeerId& src) {
		NetAddress retval;
		memcpy_s(&retval, sizeof(NetAddress), &src, sizeof(PeerId));
		memset(((BYTE*) &retval) + sizeof(PeerId), 0, sizeof(NetAddress) - sizeof(PeerId));
		return retval;
	}

	void passAdvertisement(const PeerId& host, Util::MemoryFrame ad) {
		snp::passAdvertisement(host, ad);
	}

	void removeAdvertisement(const PeerId& host) {
		snp::removeAdvertisement(host);
	}

	void passPacket(const PeerId& host, Util::MemoryFrame packet) {
		snp::passPacket(host, packet);
	}

	virtual void initialize() = 0;
	virtual void destroy() = 0;
	virtual void requestAds() = 0;
	virtual void sendAsyn(const PeerId& to, Util::MemoryFrame packet) = 0;
	virtual void receive() = 0;
	virtual void startAdvertising(Util::MemoryFrame ad) = 0;
	virtual void stopAdvertising() = 0;
};

using BinNetwork = Network<NetAddress>;

}
