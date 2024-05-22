#pragma once

#include "Util/MemoryFrame.h"
#include "Util/Types.h"
#include "Storm/storm.h"

constexpr auto MAX_PACKET_SIZE = 500;



//
// The Network interface separates the Storm stuff from pure networking
//

namespace snp {
	const int PACKET_SIZE = MAX_PACKET_SIZE;

	struct NetworkInfo {
		char* pszName;
		DWORD         dwIdentifier;
		char* pszDescription;
		CAPS          caps;
	};

	typedef ::SOCKADDR SOCKADDR;
	//typedef ::SNETADDR SNETADDR;
	extern void passAdvertisement(const SNetAddr& host, Util::MemoryFrame ad);
	extern void removeAdvertisement(const SNetAddr& host);
	//extern void passPacket(const SNETADDR& host, Util::MemoryFrame packet);
	extern void passPacket(GamePacket& parket);

	template<typename PEERID>
	class Network {
	public:
		Network() {
		}
		virtual ~Network() {
		}

		SNetAddr makeBin(const PEERID& src) {
			SNetAddr retval;
			memcpy_s(&retval, sizeof(SNetAddr), &src, sizeof(PEERID));
			memset(((BYTE*) &retval) + sizeof(PEERID), 0, sizeof(SNetAddr) - sizeof(PEERID));
			return retval;
		}

		// callback functions that take network specific arguments and cast them away
		void passAdvertisement(const PEERID& host, Util::MemoryFrame ad) {
			snp::passAdvertisement(host, ad);
		}
		void removeAdvertisement(const PEERID& host) {
			snp::removeAdvertisement(host);
		}
		void passPacket(const PEERID& host, Util::MemoryFrame packet) {
			snp::passPacket(host, packet);
			//SNP::passPacket(makeBin(host), packet);
		}

		// network plug functions
		virtual void initialize() = 0;
		virtual void destroy() = 0;
		virtual void requestAds() = 0;
		virtual void sendAsyn(const PEERID& to, Util::MemoryFrame packet) = 0;
		virtual void receive() = 0;
		virtual void startAdvertising(Util::MemoryFrame ad) = 0;
		virtual void stopAdvertising() = 0;
	};

	typedef Network<SNetAddr> BinNetwork;
}
