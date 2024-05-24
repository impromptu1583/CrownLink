#include "SNPModule.h"

#include "bwapi/CriticalSection.h"
#include "bwapi/Output.h"
#include "bwapi/Util/MemoryFrame.h"
#include "bwapi/Storm/storm.h"
#include "signaling.h"
#include <queue>
#include <list>

namespace snp {

client_info g_game_app_info;

CriticalSection g_crit_sec;
CriticalSection::Lock* g_crit_sec_ex_lock = NULL;
#define INTERLOCKED CriticalSection::Lock crit_sec_lock{g_crit_sec};

std::queue<GamePacket> g_incoming_game_packets;

std::list<AdFile> g_game_list;
int g_next_game_ad_id = 1;

AdFile g_hosted_game;

void passAdvertisement(const SNetAddr& host, Util::MemoryFrame ad) {
	INTERLOCKED;

	AdFile* adFile = nullptr;
	for (auto& g : g_game_list) {
		if (!memcmp(&g.game_info.saHost, &host, sizeof(SNetAddr))) {
			adFile = &g;
			break;
		}
	}

	if (!adFile) {
		AdFile g;
		g_game_list.push_back(g);
		adFile = &g_game_list.back();
		adFile->game_info.dwIndex = ++g_next_game_ad_id;
	}

	int indx = adFile->game_info.dwIndex;
	Util::MemoryFrame::from(adFile->game_info).writeAs(ad.readAs<game>()); // this overwrites the index lol
	Util::MemoryFrame::from(adFile->extra_bytes).write(ad);

	auto game_name_prefixes = std::string();

	if (g_game_app_info.dwVerbyte != adFile->game_info.dwVersion) {
		g_root_logger.info("version byte mismatch. ours: {} theirs: {}",
				g_game_app_info.dwVerbyte, adFile->game_info.dwVersion);
		game_name_prefixes.append("[!ver]");
	}

	switch (g_juice_manager.peer_status(host)) {
		case JUICE_STATE_CONNECTING:{
			game_name_prefixes.append("[P2P Connecting]");
		}break;
		case JUICE_STATE_FAILED:
		{
			game_name_prefixes.append("[P2P Failed]");
		}
	}

	if (game_name_prefixes.size()) {
		game_name_prefixes.append(adFile->game_info.szGameName);
		if (game_name_prefixes.size() > 128) {
			game_name_prefixes = game_name_prefixes.substr(0, 128);
		}
		memcpy_s(adFile->game_info.szGameName, sizeof(adFile->game_info.szGameName),
				game_name_prefixes.c_str(),game_name_prefixes.size());
	}


	adFile->game_info.dwTimer = GetTickCount();
	adFile->game_info.saHost = host;
	adFile->game_info.pExtra = adFile->extra_bytes;
	adFile->game_info.dwIndex = indx;
}

void removeAdvertisement(const SNetAddr& host) {}

void passPacket(GamePacket& packet) {
	INTERLOCKED;
	g_incoming_game_packets.push(packet);
	SetEvent(g_receive_event);
}

BOOL __stdcall spiInitialize(client_info* gameClientInfo, user_info* userData, battle_info* bnCallbacks, module_info* moduleData, HANDLE hEvent) {
	g_game_app_info = *gameClientInfo;
	g_receive_event = hEvent;
	g_crit_sec.init();

	try {
		g_plugged_network->initialize();
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return FALSE;
	}

	return TRUE;
}

BOOL __stdcall spiDestroy() {
	try {
		g_plugged_network->destroy();
		g_plugged_network.reset();
	} catch (GeneralException& e) {
		g_root_logger.error("unhandled exception in spiDestroy {}",e.getMessage());
		return FALSE;
	}

	return TRUE;
}

BOOL __stdcall spiLockGameList(int a1, int a2, game** ppGameList) {
	g_crit_sec_ex_lock = new CriticalSection::Lock(g_crit_sec);

	AdFile* lastAd = nullptr;

	auto a = g_game_list.size();
	for (auto& it : g_game_list) {
		it.game_info.pExtra = it.extra_bytes;
		if (lastAd)
			lastAd->game_info.pNext = &it.game_info;

		lastAd = &it;
	}

	if (lastAd)
		lastAd->game_info.pNext = nullptr;

	std::erase_if(g_game_list, [now = GetTickCount()](const auto& current_ad) { return now > current_ad.game_info.dwTimer + 2000; });

	try {
		// return game list
		*ppGameList = nullptr;
		if (!g_game_list.empty())
			*ppGameList = &g_game_list.begin()->game_info;
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return FALSE;
	}
	return TRUE;
}

DWORD gdwLastTickCount;

BOOL __stdcall spiUnlockGameList(game* pGameList, DWORD* a2) {
	delete g_crit_sec_ex_lock;
	g_crit_sec_ex_lock = NULL;

	try {
		g_plugged_network->requestAds();
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return FALSE;
	}

	return TRUE;
}

BOOL __stdcall spiStartAdvertisingLadderGame(char* pszGameName, char* pszGamePassword, char* pszGameStatString, DWORD dwGameState, DWORD dwElapsedTime, DWORD dwGameType, int a7, int a8, void* pExtraBytes, DWORD dwExtraBytesCount) {
	INTERLOCKED;

	memset(&g_hosted_game, 0, sizeof(g_hosted_game));
	strcpy_s(g_hosted_game.game_info.szGameName, sizeof(g_hosted_game.game_info.szGameName), pszGameName);
	strcpy_s(g_hosted_game.game_info.szGameStatString, sizeof(g_hosted_game.game_info.szGameStatString), pszGameStatString);
	g_hosted_game.game_info.dwGameState = dwGameState;
	g_hosted_game.game_info.dwProduct = g_game_app_info.dwProduct;
	g_hosted_game.game_info.dwVersion = g_game_app_info.dwVerbyte;
	g_hosted_game.game_info.dwUnk_1C = 0x0050;
	g_hosted_game.game_info.dwUnk_24 = 0x00a7;

	memcpy(g_hosted_game.extra_bytes, pExtraBytes, dwExtraBytesCount);
	g_hosted_game.game_info.dwExtraBytes = dwExtraBytesCount;
	g_hosted_game.game_info.pExtra = g_hosted_game.extra_bytes;

	g_plugged_network->startAdvertising(Util::MemoryFrame::from(g_hosted_game));
	return TRUE;
}

BOOL __stdcall spiStopAdvertisingGame() {
	INTERLOCKED;
	g_plugged_network->stopAdvertising();
	return TRUE;
}

BOOL __stdcall spiGetGameInfo(DWORD dwFindIndex, char* pszFindGameName, int a3, game* pGameResult) {
	INTERLOCKED;

	for (auto& it : g_game_list) {
		if (it.game_info.dwIndex == dwFindIndex) {
			*pGameResult = it.game_info;
			return TRUE;
		}
	}

	SErrSetLastError(STORM_ERROR_GAME_NOT_FOUND);
	return FALSE;
}

BOOL __stdcall spiSend(DWORD addrCount, SNetAddr** addrList, char* buf, DWORD bufLen) {
	if (!addrCount) {
		return TRUE;
	}

	if (addrCount > 1) {
		DropMessage(1, "spiSend, multicast not supported");
	}

	try {
		SNetAddr him = *(addrList[0]);
		std::string tmp;
		tmp.append(buf, bufLen);
		g_root_logger.trace("spiSend: {}", tmp);

		g_plugged_network->sendAsyn(him, Util::MemoryFrame(buf, bufLen));
	} catch (GeneralException& e) {
		DropLastError("spiSend failed: %s", e.getMessage());
		return FALSE;
	}
	return TRUE;
}

BOOL __stdcall spiReceive(SNetAddr** senderPeer, char** data, DWORD* databytes) {
	INTERLOCKED;

	*senderPeer = nullptr;
	*data = nullptr;
	*databytes = 0;

	try {

		while (true) {
			GamePacket* loan = new GamePacket();
			if (!receive_queue.try_pop_dont_wait(*loan)) {
				SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
				return FALSE;
			}
			std::string debugstr(loan->data, loan->size);
			g_root_logger.trace("spiReceive: {} :: {}", loan->timestamp, debugstr);

			if (GetTickCount() > loan->timestamp + 10000) {
				DropMessage(1, "Dropped outdated packet (%dms delay)", GetTickCount() - loan->timestamp);
				continue;
			}

			*senderPeer = &loan->sender;
			*data = loan->data;
			*databytes = loan->size;

			break;
		}
	} catch (GeneralException& e) {
		DropLastError("spiLockGameList failed: %s", e.getMessage());
		return FALSE;
	}
	return TRUE;
}

BOOL __stdcall spiFree(SNetAddr* addr, char* data, DWORD databytes) {
	INTERLOCKED;

	BYTE* loan = (BYTE*) addr;
	if (loan) {
		delete loan;
	}
	return TRUE;
}

BOOL __stdcall spiCompareNetAddresses(SNetAddr* addr1, SNetAddr* addr2, DWORD* dwResult) {
	INTERLOCKED;

	DropMessage(0, "spiCompareNetAddresses");
	if (dwResult) {
		*dwResult = 0;
	}
	if (!addr1 || !addr2 || !dwResult) {
		SErrSetLastError(ERROR_INVALID_PARAMETER);
		return FALSE;
	}

	*dwResult = (0 == memcmp(addr1, addr2, sizeof(SNetAddr)));
	return TRUE;
}

BOOL __stdcall spiLockDeviceList(DWORD* a1) {
	*a1 = 0;
	return TRUE;
}

BOOL __stdcall spiUnlockDeviceList(void* unknownStruct) {
	return TRUE;
}

BOOL __stdcall spiFreeExternalMessage(SNetAddr* addr, char* data, DWORD databytes) {
	DropMessage(0, "spiFreeExternalMessage");
	return FALSE;
}

BOOL __stdcall spiGetPerformanceData(DWORD dwType, DWORD* dwResult, int a3, int a4) {
	DropMessage(0, "spiGetPerformanceData");
	return FALSE;
}

BOOL __stdcall spiInitializeDevice(int a1, void* a2, void* a3, DWORD* a4, void* a5) {
	DropMessage(0, "spiInitializeDevice");
	return FALSE;
}

BOOL __stdcall spiReceiveExternalMessage(SNetAddr** addr, char** data, DWORD* databytes) {
	if (addr) {
		*addr = NULL;
	}
	if (data) {
		*data = NULL;
	}
	if (databytes) {
		*databytes = 0;
	}
	SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
	return FALSE;
}

BOOL __stdcall spiSelectGame(int a1,
	client_info* gameClientInfo,
	user_info* userData,
	battle_info* bnCallbacks,
	module_info* moduleData,
	int a6) {
	DropMessage(0, "spiSelectGame");
	// Looks like an old function and doesn't seem like it's used anymore
	// UDPN's function Creates an IPX game select dialog window
	return FALSE;
}

BOOL __stdcall spiSendExternalMessage(int a1, int a2, int a3, int a4, int a5) {
	DropMessage(0, "spiSendExternalMessage");
	return FALSE;
}

BOOL __stdcall spiLeagueGetName(char* pszDest, DWORD dwSize) {
	DropMessage(0, "spiLeagueGetName");
	return TRUE;
}

snp::NetFunctions g_spi_functions = {
	  sizeof(snp::NetFunctions),
/*n*/ &snp::spiCompareNetAddresses,
	  &snp::spiDestroy,
	  &snp::spiFree,
/*e*/ &snp::spiFreeExternalMessage,
      &snp::spiGetGameInfo,
/*n*/ &snp::spiGetPerformanceData,
      &snp::spiInitialize,
/*e*/ &snp::spiInitializeDevice,
/*e*/ &snp::spiLockDeviceList,
      &snp::spiLockGameList,
      &snp::spiReceive,
/*n*/ &snp::spiReceiveExternalMessage,
/*e*/ &snp::spiSelectGame,
      &snp::spiSend,
/*e*/ &snp::spiSendExternalMessage,
/*n*/ &snp::spiStartAdvertisingLadderGame,
/*n*/ &snp::spiStopAdvertisingGame,
/*e*/ &snp::spiUnlockDeviceList,
      &snp::spiUnlockGameList,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
	  NULL,
/*n*/ &snp::spiLeagueGetName
};

}
