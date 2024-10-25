#include "SNPModule.h"

#include "CrownLink.h"
#include <list>

namespace snp {

struct SNPContext {
	client_info game_app_info;

	std::list<AdFile> game_list;
	s32 next_game_ad_id = 1;

	AdFile hosted_game;
	AdFile status_ad;
	bool   status_ad_used = false;
};

static SNPContext g_snp_context;

void pass_advertisement(const NetAddress& host, AdFile& ad) {
	std::lock_guard lock{g_advertisement_mutex};

	AdFile* adFile = nullptr;
	for (auto& game : g_snp_context.game_list) {
		if (!memcmp(&game.game_info.host, &host, sizeof(NetAddress))) {
			adFile = &game;
			break;
		}
	}

	if (!adFile) {
		adFile = &g_snp_context.game_list.emplace_back();
		ad.game_info.game_index = ++g_snp_context.next_game_ad_id;
	} else {
		ad.game_info.game_index = adFile->game_info.game_index;
	}

	memcpy_s(adFile, sizeof(AdFile), &ad, sizeof(AdFile));

	std::string prefix;
	if (g_snp_context.game_app_info.version_id != adFile->game_info.version_id) {
		spdlog::info("Version byte mismatch. ours: {} theirs: {}", g_snp_context.game_app_info.version_id, adFile->game_info.version_id);
		prefix += "[!Ver]";
	}
	try {
		if (adFile->crownlink_mode == CrownLinkMode::DBCL) {
			prefix += "[DBC]";
		}
		if (adFile->crownlink_mode != g_crown_link->mode()) {
			adFile->game_info.version_id = 0; // should ensure we can't join the game
		}
	} catch (const std::exception& e){
		spdlog::error("error comparing crownlink versions, peer may be on an old version. Exception: {}", e.what());
	}

	switch (g_crown_link->juice_manager().agent_state(host)) {
        case JUICE_STATE_CONNECTING: {
            prefix += "[P2P Connecting]";
        } break;
        case JUICE_STATE_FAILED: {
            prefix += "[P2P Failed]";
        } break;
        case JUICE_STATE_DISCONNECTED: {
            prefix += "[P2P Not Connected]";
        } break;
	}

	switch (g_crown_link->juice_manager().final_connection_type(host)) {
        case JuiceConnectionType::Relay: {
            prefix += "[Relayed]";
        } break;
        case JuiceConnectionType::Radmin: {
            prefix += "[Radmin]";
        } break;
	}

	prefix += adFile->game_info.game_name;
	if (prefix.size() > 23) { prefix.resize(23); }


	// todo - make this optional via config file
	// map name is in description after carriage return character
	std::string description(adFile->game_info.game_description);
	std::string map_name;
	auto pos = description.find('\r');
	if (pos != std::string::npos) {
		map_name = description.substr(pos);
		map_name.erase(map_name.find_last_not_of("\t\n\v\f\r ") + 1);
	}

	auto total_length = prefix.size() + map_name.size();
	if (total_length > 22) {
		auto to_trim = total_length - 22;
		if (map_name.size() > 8) {
			auto map_trim = (std::min)(to_trim, map_name.size() - 8);
			map_name.resize(map_name.size() - map_trim);
			to_trim -= map_trim;
		}
		if (to_trim > 0) {
			prefix.resize(prefix.size() - to_trim);
		}
	}
	prefix += "-" + map_name;

	strncpy_s(adFile->game_info.game_name, sizeof(adFile->game_info.game_name), prefix.c_str(), sizeof(adFile->game_info.game_name));
	
	adFile->game_info.host_last_time = GetTickCount();
	adFile->game_info.host = *(NetAddress*)&host;
	adFile->game_info.pExtra = adFile->extra_bytes;
}

void remove_advertisement(const NetAddress& host) {}

void pass_packet(GamePacket& packet) {}

static void init_logging() {
	const auto& snp_config = SnpConfig::instance();
	spdlog::init_thread_pool(8192, 1);

	const auto log_filename = (g_starcraft_dir / "crownlink_logs" / "CrownLink.txt").generic_wstring();
	auto standard_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 2, 30);
	standard_sink->set_level(spdlog::level::debug);

	const auto trace_filename = (g_starcraft_dir / "crownlink_logs" / "CLTrace.txt").generic_wstring();
	auto trace_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(trace_filename, 1024*1024*10, 3);
	trace_sink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::async_logger>("Crownlink", std::initializer_list<spdlog::sink_ptr>{std::move(standard_sink), std::move(trace_sink)}, spdlog::thread_pool(), spdlog::async_overflow_policy::block);
	spdlog::register_logger(logger);
	logger->flush_on(spdlog::level::debug);
	spdlog::set_default_logger(logger);

	switch (snp_config.log_level) {
		case LogLevel::Trace: {
			spdlog::set_level(spdlog::level::trace);
		} break;
		case LogLevel::Debug: {
			spdlog::set_level(spdlog::level::debug);
		} break;
		case LogLevel::Info: {
			spdlog::set_level(spdlog::level::info);
			spdlog::enable_backtrace(32);
		} break;
		case LogLevel::Warn: {
			spdlog::set_level(spdlog::level::warn);
			spdlog::enable_backtrace(32);
		} break;
		case LogLevel::Error: {
			spdlog::set_level(spdlog::level::err);
			spdlog::enable_backtrace(32);
		} break;
		case LogLevel::Fatal: {
            spdlog::set_level(spdlog::level::critical);
			spdlog::enable_backtrace(32);
        } break;
		case LogLevel::None: {
			spdlog::set_level(spdlog::level::off);
			spdlog::enable_backtrace(32);
		} break;
	}
	spdlog::set_level(spdlog::level::trace); // For fraudarchy debug
}

static BOOL __stdcall spi_initialize(client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_data, HANDLE event) {
	g_snp_context.game_app_info = *client_info;
	g_receive_event = event;
	init_logging();

	set_status_ad("Crownlink Initializing");
	spdlog::info("Crownlink Initializing, mode: {}, game version: {}",to_string(g_crown_link->mode()),g_snp_context.game_app_info.version_id);

	auto mode = g_crown_link->mode();
	try {
		g_crown_link = std::make_unique<CrownLink>();
		g_crown_link->set_mode(mode);
	} catch (const std::exception& e) {
		spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}

	return true;
}

static BOOL __stdcall spi_destroy() {
	spdlog::debug("spi_destroy called");
	try {
		g_crown_link.reset();
	} catch (const std::exception& e) {
		spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	spdlog::shutdown();

	return true;
}

static BOOL __stdcall spi_lock_game_list(int, int, game** out_game_list) {
	std::lock_guard lock{g_advertisement_mutex};

	std::erase_if(g_snp_context.game_list, [now = GetTickCount()](const auto& current_ad) { return now > current_ad.game_info.host_last_time + 2000; });

	AdFile* last_ad = nullptr;
	for (auto& game : g_snp_context.game_list) {
		game.game_info.pExtra = game.extra_bytes;
		if (last_ad) {
			last_ad->game_info.pNext = &game.game_info;
		}

		last_ad = &game;
	}
	if (last_ad) {
		last_ad->game_info.pNext = nullptr;
	}

	if (last_ad && g_snp_context.status_ad_used) {
		last_ad->game_info.pNext = &g_snp_context.status_ad.game_info;
		g_snp_context.status_ad.game_info.game_index = last_ad->game_info.game_index + 1;
	}

	try {
		*out_game_list = nullptr;
		if (!g_snp_context.game_list.empty()) {
			
			*out_game_list = &g_snp_context.game_list.begin()->game_info;
		}
		else if(g_snp_context.status_ad_used) {
			*out_game_list = &g_snp_context.status_ad.game_info;
		}
	} catch (const std::exception& e) {
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

static BOOL __stdcall spi_unlock_game_list(game* game_list, DWORD*) {
	try {
		g_crown_link->request_advertisements();
	} catch (const std::exception& e) {
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}

	return true;
}

static void create_ad(AdFile& ad_file, const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data, DWORD user_data_size) {
	memset(&ad_file, 0, sizeof(ad_file));
	ad_file.crownlink_mode = g_crown_link->mode();
	auto& game_info = ad_file.game_info;
	strcpy_s(game_info.game_name, sizeof(game_info.game_name), game_name);
	strcpy_s(game_info.game_description, sizeof(game_info.game_description), game_stat_string);
	game_info.game_state = game_state;
	game_info.program_id = g_snp_context.game_app_info.program_id;
	game_info.version_id = g_snp_context.game_app_info.version_id;
	game_info.host_latency = 0x0050;
	game_info.category_bits = 0x00a7;

	memcpy(ad_file.extra_bytes, user_data, user_data_size);
	game_info.extra_bytes = user_data_size;
	game_info.pExtra = ad_file.extra_bytes;
}

void set_status_ad(const std::string& status) {
	std::lock_guard lock{g_advertisement_mutex};
	auto statstr = std::string{",33,,3,,1e,,1,cb2edaab,5,,Server\rStatus\r"};
	char user_data[32] = {
		12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
	};
	create_ad(g_snp_context.status_ad, status.c_str(), statstr.c_str(), 0, &user_data, 32);
	g_snp_context.status_ad.game_info.pNext = nullptr;
	g_snp_context.status_ad_used = true;
}

void clear_status_ad() {
	g_snp_context.status_ad_used = false;
}

static BOOL __stdcall spi_start_advertising_ladder_game(char* game_name, char* game_password, char* game_stat_string, DWORD game_state, DWORD elapsed_time, DWORD game_type, int, int, void* user_data, DWORD user_data_size) {
	std::lock_guard lock{g_advertisement_mutex};
	create_ad(g_snp_context.hosted_game, game_name, game_stat_string, game_state, user_data, user_data_size);
	g_crown_link->start_advertising(g_snp_context.hosted_game);
	return true;
}

static BOOL __stdcall spi_stop_advertising_game() {
	std::lock_guard lock{g_advertisement_mutex};
	g_crown_link->stop_advertising();
	return true;
}

static BOOL __stdcall spi_get_game_info(DWORD index, char* game_name, int, game* out_game) {
	std::lock_guard lock{g_advertisement_mutex};

	for (auto& game : g_snp_context.game_list) {
		if (game.game_info.game_index == index) {
			*out_game = game.game_info;
			return true;
		}
	}

	// SErrSetLastError(STORM_ERROR_GAME_NOT_FOUND);
	return false;
}

static BOOL __stdcall spi_send(DWORD address_count, NetAddress** out_address_list, char* data, DWORD size) {
	if (!address_count) {
		spdlog::error("spisend with no address listed");
		return true;
	}

	if (address_count > 1) {
		spdlog::info("multicast attempted");
	}

	try {
		const NetAddress& peer = out_address_list[0][0];
		spdlog::trace("spiSend to {}: {:pa}", peer.b64(), spdlog::to_hex(std::string{data, size}));
		g_crown_link->send(peer, data, size);
	} catch (const std::exception& e) {
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

static BOOL __stdcall spi_receive(NetAddress** peer, char** out_data, DWORD* out_size) {
	*peer = nullptr;
	*out_data = nullptr;
	*out_size = 0;

	GamePacket* loan = new GamePacket{};
	try {
		while (true) {
			if (!g_crown_link->receive_queue().try_dequeue(*loan)) {
				return false;
			}
			std::string debug_string{loan->data, loan->size};
			// spdlog::trace("spiReceive: {} :: {}", loan->timestamp, debug_string);
			spdlog::trace("spiRecv fr {}: {:pa}", loan->sender.b64(), spdlog::to_hex(std::string{ loan->data,loan->size }));
			if (GetTickCount() > loan->timestamp + 10000) {
				spdlog::trace("discarding old packet");
				continue;
			}

			*peer = &loan->sender;
			*out_data = loan->data;
			*out_size = loan->size;

			break;
		}
	} catch (std::exception& e) {
		delete loan;
		spdlog::dump_backtrace();
		spdlog::error("unhandled error {} in {}", e.what(), __FUNCSIG__);
		return false;
	}
	return true;
}

static BOOL __stdcall spi_free(NetAddress* loan, char* data, DWORD size) {
	if (loan) {
		delete loan;
	}
	return true;
}

static BOOL __stdcall spi_compare_net_addresses(NetAddress* address1, NetAddress* address2, DWORD* out_result) {
	if (out_result) {
		*out_result = 0;
	}
	if (!address1 || !address2 || !out_result) {
		// SErrSetLastError(ERROR_INVALID_PARAMETER);
		return false;
	}

	*out_result = (memcmp(address1, address2, sizeof(NetAddress)) == 0);
	return true;
}

static BOOL __stdcall spi_lock_device_list(DWORD* out_device_list) {
	*out_device_list = 0;
	return true;
}

static BOOL __stdcall spi_unlock_device_list(void*) {
	return true;
}

static BOOL __stdcall spi_free_external_message(NetAddress* address, char* data, DWORD size) {
	return false;
}

static BOOL __stdcall spi_get_performance_data(DWORD type, DWORD* out_result, int, int) {
	return false;
}

static BOOL __stdcall spi_initialize_device(int, void*, void*, DWORD*, void*) {
	return false;
}

static BOOL __stdcall spi_receive_external_message(NetAddress** out_address, char** out_data, DWORD* out_size) {
	if (out_address) {
		*out_address = nullptr;
	}
	if (out_data) {
		*out_data = nullptr;
	}
	if (out_size) {
		*out_size = 0;
	}
	// SErrSetLastError(STORM_ERROR_NO_MESSAGES_WAITING);
	return false;
}

static BOOL __stdcall spi_select_game(int, client_info* client_info, user_info* user_info, battle_info* callbacks, module_info* module_info, int) {
	// Looks like an old function and doesn't seem like it's used anymore
	// UDPN's function Creates an IPX game select dialog window
	return false;
}

static BOOL __stdcall spi_send_external_message(int, int, int, int, int) {
	return false;
}

static BOOL __stdcall spi_league_get_name(char* data, DWORD size) {
	return true;
}

snp::NetFunctions g_spi_functions = {
	  sizeof(snp::NetFunctions),
/*n*/ &snp::spi_compare_net_addresses,
	  &snp::spi_destroy,
	  &snp::spi_free,
/*e*/ &snp::spi_free_external_message,
      &snp::spi_get_game_info,
/*n*/ &snp::spi_get_performance_data,
      &snp::spi_initialize,
/*e*/ &snp::spi_initialize_device,
/*e*/ &snp::spi_lock_device_list,
      &snp::spi_lock_game_list,
      &snp::spi_receive,
/*n*/ &snp::spi_receive_external_message,
/*e*/ &snp::spi_select_game,
      &snp::spi_send,
/*e*/ &snp::spi_send_external_message,
/*n*/ &snp::spi_start_advertising_ladder_game,
/*n*/ &snp::spi_stop_advertising_game,
/*e*/ &snp::spi_unlock_device_list,
      &snp::spi_unlock_game_list,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
	  nullptr,
/*n*/ &snp::spi_league_get_name
};

}
