#include "SNPModule.h"

#include "bwapi/CriticalSection.h"
#include "bwapi/Output.h"
#include "bwapi/Util/MemoryFrame.h"
#include "bwapi/Storm/storm.h"
#include "CrownLink.h"
#include <queue>
#include <list>

namespace snp {

struct SNPContext {
	client_info game_app_info;

	std::list<AdFile> game_list;
	s32 next_game_ad_id = 1;

	AdFile hosted_game;
};

static SNPContext g_snp_context;

void passAdvertisement(const NetAddress& host, Util::MemoryFrame ad) {
	std::lock_guard lock{g_advertisement_mutex};

	AdFile* adFile = nullptr;
	for (auto& game : g_snp_context.game_list) {
		if (!memcmp(&game.game_info.saHost, &host, sizeof(NetAddress))) {
			adFile = &game;
			break;
		}
	}

	if (!adFile) {
		adFile = &g_snp_context.game_list.emplace_back();
		adFile->game_info.dwIndex = ++g_snp_context.next_game_ad_id;
	}

	s32 index = adFile->game_info.dwIndex;
	Util::MemoryFrame::from(adFile->game_info).writeAs(ad.readAs<game>()); // this overwrites the index lol
	Util::MemoryFrame::from(adFile->extra_bytes).write(ad);

	std::string prefix;
	if (g_snp_context.game_app_info.dwVerbyte != adFile->game_info.dwVersion) {
		Logger::root().info("Version byte mismatch. ours: {} theirs: {}", g_snp_context.game_app_info.dwVerbyte, adFile->game_info.dwVersion);
		prefix += "[!Ver]";
	}

	switch (g_crown_link->juice_manager().agent_state(host)) {
	case JUICE_STATE_CONNECTING:{
		prefix += "[P2P Connecting]";
	} break;
	case JUICE_STATE_FAILED: {
		prefix += "[P2P Failed]";
	} break;
	}

	if (g_crown_link->juice_manager().is_relayed(host)) {
		prefix += "[R]";
	}

	if (!prefix.empty()) {
		prefix += " ";
		prefix += adFile->game_info.szGameName;
		strncpy(adFile->game_info.szGameName, prefix.c_str(), sizeof(adFile->game_info.szGameName));
	}

	adFile->game_info.dwTimer = GetTickCount();
	adFile->game_info.saHost = *(SNETADDR*)&host;
	adFile->game_info.pExtra = adFile->extra_bytes;
	adFile->game_info.dwIndex = index;
}

void removeAdvertisement(const NetAddress& host) {}

void passPacket(GamePacket& packet) {}

BOOL __stdcall spiInitialize(client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_data, HANDLE event) {
	g_snp_context.game_app_info = *client_info;
	g_receive_event = event;

	try {
		g_crown_link = std::make_unique<CrownLink>();
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return false;
	}

	return true;
}

BOOL __stdcall spiDestroy() {
	try {
		g_crown_link.reset();
	} catch (GeneralException& e) {
		Logger::root().error("unhandled exception in spiDestroy {}",e.getMessage());
		return false;
	}

	return true;
}

BOOL __stdcall spiLockGameList(int, int, game** out_game_list) {
	std::lock_guard lock{g_advertisement_mutex};

	AdFile* lastAd = nullptr;
	for (auto& game : g_snp_context.game_list) {
		game.game_info.pExtra = game.extra_bytes;
		if (lastAd) {
			lastAd->game_info.pNext = &game.game_info;
		}

		lastAd = &game;
	}
	if (lastAd) {
		lastAd->game_info.pNext = nullptr;
	}
	std::erase_if(g_snp_context.game_list, [now = GetTickCount()](const auto& current_ad) { return now > current_ad.game_info.dwTimer + 2000; });

	try {
		*out_game_list = nullptr;
		if (!g_snp_context.game_list.empty()) {
			*out_game_list = &g_snp_context.game_list.begin()->game_info;
		}
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return false;
	}
	return true;
}

BOOL __stdcall spiUnlockGameList(game* game_list, DWORD*) {
	try {
		g_crown_link->request_advertisements();
	} catch (GeneralException& e) {
		DropLastError(__FUNCTION__ " unhandled exception: %s", e.getMessage());
		return false;
	}

	return true;
}

BOOL __stdcall spiStartAdvertisingLadderGame(char* game_name, char* game_password, char* game_stat_string, DWORD game_state, DWORD elapsed_time, DWORD game_type, int, int, void* user_data, DWORD user_data_size) {
	std::lock_guard lock{g_advertisement_mutex};

	auto& hosted_game = g_snp_context.hosted_game;
	memset(&hosted_game, 0, sizeof(hosted_game));

	auto& game_info = hosted_game.game_info;
	strcpy_s(game_info.szGameName, sizeof(game_info.szGameName), game_name);
	strcpy_s(game_info.szGameStatString, sizeof(game_info.szGameStatString), game_stat_string);
	game_info.dwGameState = game_state;
	game_info.dwProduct = g_snp_context.game_app_info.dwProduct;
	game_info.dwVersion = g_snp_context.game_app_info.dwVerbyte;
	game_info.dwUnk_1C = 0x0050;
	game_info.dwUnk_24 = 0x00a7;

	memcpy(hosted_game.extra_bytes, user_data, user_data_size);
	game_info.dwExtraBytes = user_data_size;
	game_info.pExtra = hosted_game.extra_bytes;

	g_crown_link->start_advertising(Util::MemoryFrame::from(g_snp_context.hosted_game));
	return true;
}

BOOL __stdcall spiStopAdvertisingGame() {
	std::lock_guard lock{g_advertisement_mutex};
	g_crown_link->stop_advertising();
	return true;
}

BOOL __stdcall spiGetGameInfo(DWORD index, char* game_name, int, game* out_game) {
	std::lock_guard lock{g_advertisement_mutex};

	for (auto& game : g_snp_context.game_list) {
		if (game.game_info.dwIndex == index) {
			*out_game = game.game_info;
			return true;
		}
	}

	SErrSetLastError(STORM_ERROR_GAME_NOT_FOUND);
	return false;
}

BOOL __stdcall spiSend(DWORD address_count, NetAddress** out_address_list, char* data, DWORD size) {
	if (!address_count) {
		return true;
	}

	if (address_count > 1) {
		DropMessage(1, "spiSend, multicast not supported");
	}

	try {
		NetAddress peer = *(out_address_list[0]);
		Logger::root().trace("spiSend: {}", std::string{data, size});

		g_crown_link->send(peer, data, size);
	} catch (GeneralException& e) {
		DropLastError("spiSend failed: %s", e.getMessage());
		return false;
	}
	return true;
}

BOOL __stdcall spiReceive(NetAddress** peer, char** out_data, DWORD* out_size) {
	*peer = nullptr;
	*out_data = nullptr;
	*out_size = 0;

	try {
		while (true) {
			GamePacket* loan = new GamePacket();
			if (!g_crown_link->receive_queue().try_pop_dont_wait(*loan)) {
				SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
				return false;
			}
			std::string debug_string(loan->data, loan->size);
			Logger::root().trace("spiReceive: {} :: {}", loan->timestamp, debug_string);

			if (GetTickCount() > loan->timestamp + 10000) {
				DropMessage(1, "Dropped outdated packet (%dms delay)", GetTickCount() - loan->timestamp);
				delete loan;
				continue;
			}

			*peer = &loan->sender;
			*out_data = loan->data;
			*out_size = loan->size;

			break;
		}
	} catch (GeneralException& e) {
		DropLastError("spiLockGameList failed: %s", e.getMessage());
		return false;
	}
	return true;
}

BOOL __stdcall spiFree(NetAddress* loan, char* data, DWORD size) {
	if (loan) {
		delete loan;
	}
	return true;
}

BOOL __stdcall spiCompareNetAddresses(NetAddress* address1, NetAddress* address2, DWORD* out_result) {
	DropMessage(0, "spiCompareNetAddresses");
	if (out_result) {
		*out_result = 0;
	}
	if (!address1 || !address2 || !out_result) {
		SErrSetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*out_result = (memcmp(address1, address2, sizeof(NetAddress)) == 0);
	return true;
}

BOOL __stdcall spiLockDeviceList(DWORD* out_device_list) {
	*out_device_list = 0;
	return true;
}

BOOL __stdcall spiUnlockDeviceList(void*) {
	return true;
}

BOOL __stdcall spiFreeExternalMessage(NetAddress* address, char* data, DWORD size) {
	DropMessage(0, "spiFreeExternalMessage");
	return false;
}

BOOL __stdcall spiGetPerformanceData(DWORD type, DWORD* out_result, int, int) {
	DropMessage(0, "spiGetPerformanceData");
	return false;
}

BOOL __stdcall spiInitializeDevice(int, void*, void*, DWORD*, void*) {
	DropMessage(0, "spiInitializeDevice");
	return false;
}

BOOL __stdcall spiReceiveExternalMessage(NetAddress** out_address, char** out_data, DWORD* out_size) {
	if (out_address) {
		*out_address = nullptr;
	}
	if (out_data) {
		*out_data = nullptr;
	}
	if (out_size) {
		*out_size = 0;
	}
	SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
	return false;
}

BOOL __stdcall spiSelectGame(int, client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_info, int) {
	DropMessage(0, "spiSelectGame");
	// Looks like an old function and doesn't seem like it's used anymore
	// UDPN's function Creates an IPX game select dialog window
	return false;
}

BOOL __stdcall spiSendExternalMessage(int, int, int, int, int) {
	DropMessage(0, "spiSendExternalMessage");
	return false;
}

BOOL __stdcall spiLeagueGetName(char* data, DWORD size) {
	DropMessage(0, "spiLeagueGetName");
	return true;
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
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
/*n*/ &snp::spiLeagueGetName
};

}
