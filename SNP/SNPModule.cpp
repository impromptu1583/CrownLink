#include "SNPModule.h"

#include <list>

#include "CrownLink.h"

namespace snp {

struct UIParams {
    u32 flags;
    ProgramInfo* program_data;
    PlayerData* player_data;
    InterfaceData* interface_data;
    VersionInfo* version_data;
    u32* player_id;
};

struct SNPContext {
    ProgramInfo game_app_info;
    std::list<AdFile> game_list;

    std::vector<AdFile> lobbies;

    s32 next_game_ad_id = 1;
    AdFile hosted_game;
    AdFile status_ad;
    bool status_ad_used = false;

    UIParams ui_params{};

    u32 player_id;

    std::string status_string{}; 
};

static SNPContext g_snp_context;

static void init_logging() {
    const auto& snp_config = SnpConfig::instance();
    spdlog::init_thread_pool(8192, 1);

    const auto log_filename = (g_starcraft_dir / "crownlink_logs" / "CrownLink.txt").generic_wstring();
    auto standard_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 2, 30);
    standard_sink->set_level(spdlog::level::debug);

    const auto trace_filename = (g_starcraft_dir / "crownlink_logs" / "CLTrace.txt").generic_wstring();
    auto trace_sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(trace_filename, 1024 * 1024 * 10, 3);
    trace_sink->set_level(spdlog::level::trace);

    auto logger = std::make_shared<spdlog::async_logger>(
        "CL", std::initializer_list<spdlog::sink_ptr>{std::move(standard_sink), std::move(trace_sink)},
        spdlog::thread_pool(), spdlog::async_overflow_policy::block
    );
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
}

static BOOL __stdcall spi_initialize(
    ProgramInfo* client_info, PlayerData* user_info, InterfaceData* callbacks, VersionInfo* module_data, HANDLE event
) {
    // called by storm when the CrownLink connection mode is selected from the multiplayer menu
    g_snp_context.game_app_info = *client_info;
    g_receive_event = event;
    const auto& snp_config = SnpConfig::instance();
    init_logging();
    create_status_ad();
    set_status_ad("Initializing");
    HWND parwin = (HWND)callbacks->parent_window;
    //SetActiveWindow(parwin);
    //ShowWindow(parwin, SW_HIDE);


    
    try {
        g_crown_link = std::make_unique<CrownLink>();
    } catch (const std::exception& e) {
        spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
        return false;
    }

    spdlog::info(
        "Crownlink Initializing, mode: {}, game version: {}", to_string(snp_config.mode),
        g_snp_context.game_app_info.version_id
    );

    return true;
}

static BOOL __stdcall spi_destroy() {
    // called by storm when exiting out of the multiplayer lobbies / game finished screen and back to the multiplayer
    // connnection types menu

    try {
        g_crown_link.reset();
    } catch (const std::exception& e) {
        spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
        return false;
    }
    spdlog::shutdown();

    return true;
}

//===========================================================================
//
// Lobbies & Advertisements
//
//===========================================================================

void update_lobbies(std::vector<AdFile>& updated_list) {
    std::lock_guard lock{g_advertisement_mutex};

    for (AdFile& known_lobby : g_snp_context.lobbies) {
        auto it = std::ranges::find_if(updated_list.begin(), updated_list.end(), [&known_lobby](AdFile incoming_ad) {
            return known_lobby.is_same_owner(incoming_ad);
        });
        if (it != updated_list.end()) {
            known_lobby = *it;
            it->mark_for_removal = true;
        } else {
            known_lobby.mark_for_removal = true;
        }
    }
    g_snp_context.lobbies.erase(
        std::remove_if(
            g_snp_context.lobbies.begin(), g_snp_context.lobbies.end(),
            [](AdFile ad) {
                return ad.mark_for_removal;
            }
        ),
        g_snp_context.lobbies.end()
    );

    std::ranges::copy_if(updated_list, std::back_inserter(g_snp_context.lobbies), [](AdFile incoming_ad) {
        return !incoming_ad.mark_for_removal;
    });

    const auto last_updated = get_tick_count();
    u32 index = 1;
    for (AdFile& known_lobby : g_snp_context.lobbies) {
        known_lobby.game_info.host_last_time = last_updated;
        known_lobby.game_info.game_index = ++index;
        known_lobby.game_info.pExtra = known_lobby.extra_bytes;
    }
}

static std::string_view extract_map_name(const GameInfo& game_info) {
    std::string_view sv{game_info.game_description};
    sv.remove_prefix(std::min(sv.find('\r') + 1, sv.size()));
    auto back_trim_pos = sv.find_last_not_of("\t\n\v\f\r ");
    if (back_trim_pos != sv.npos) {
        sv.remove_suffix(sv.size() - back_trim_pos - 1);
    }
    return sv;
}

static BOOL __stdcall spi_lock_game_list(int, int, GameInfo** out_game_list) {
    // called by storm once per second when on the games list screen
    g_advertisement_mutex.lock();

    // storm will unlock when it's done with the data by calling spi_unlock_game_list()
    const auto& snp_config = SnpConfig::instance();

    s32 game_index = 1;
    AdFile* last_ad = nullptr;

    update_status_ad();
    last_ad = &g_snp_context.status_ad;

    std::erase_if(g_snp_context.lobbies, [](AdFile lobby) {
        return get_tick_count() - lobby.game_info.host_last_time > 3;
    });

    for (AdFile& ad : g_snp_context.lobbies) {
        bool joinable = true;
        bool has_text_prefix = false;
        std::stringstream ss;

        if (g_snp_context.game_app_info.version_id != ad.game_info.version_id) {
            joinable = false;
            has_text_prefix = true;
            ss << ColorByte::Red << "[!Ver]";
        }
        if (snp_config.mode != ad.crownlink_mode) {
            joinable = false;
            has_text_prefix = true;
            ss << ColorByte::Red << "[!Mode]";
        }
        if (ad == g_snp_context.status_ad) {
            joinable = false;
            ss << ColorByte::Red;
        }
        if (ad.game_info.game_state == 12) {
            // Game in progress - these won't be shown anyway but we don't want to initiate p2p
            joinable = false;
            ss << ColorByte::Red;
        }

        // agent_state calls ensure_agent so one will be created if needed,
        // this kicks off the p2p connection process
        if (joinable) {
            switch (g_crown_link->juice_manager().lobby_agent_state(ad)) {
                case JUICE_STATE_CONNECTING: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "...";
                } break;
                case JUICE_STATE_FAILED: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "!!";
                    g_crown_link->juice_manager().send_connection_request(ad.game_info.host);
                } break;
                case JUICE_STATE_DISCONNECTED: {
                    has_text_prefix = true;
                    ss << ColorByte::Gray << "-";
                    g_crown_link->juice_manager().send_connection_request(ad.game_info.host);
                } break;
                case JUICE_STATE_CONNECTED:
                case JUICE_STATE_COMPLETED: {
                    // Use ColorByte to manipulate menu order but revert to standard menu behavior
                    ss << ColorByte::Green << ColorByte::Revert;
                    switch (g_crown_link->juice_manager().final_connection_type(ad.game_info.host)) {
                        case JuiceConnectionType::Relay: {
                            has_text_prefix = true;
                            ss << "[R]";
                        } break;
                        case JuiceConnectionType::Radmin: {
                            has_text_prefix = true;
                            static constexpr char SadEmoji = char(131);
                            ss << std::format("[Radmin {}]", SadEmoji);
                        } break;
                        default: {
                            has_text_prefix = true;
                            static constexpr char DoubleArrow = char(187);
                            ss << DoubleArrow;
                        } break;
                    }
                }
            }
        }

        if (ad.original_name.empty()) {
            ad.original_name = ad.game_info.game_name;
        }
        ss << (has_text_prefix ? " " : "") << ad.original_name;

        if (snp_config.add_map_to_lobby_name) {
            ss << (joinable ? ColorByte::Blue : ColorByte::Revert) << " " << extract_map_name(ad.game_info);
        }

        const auto prefixes = ss.str();
        spdlog::debug("Lobby name updated to {}, length: {}", prefixes, prefixes.size());
        strncpy(ad.game_info.game_name, prefixes.c_str(), sizeof(ad.game_info.game_name));

        if (last_ad) {
            last_ad->game_info.pNext = &ad.game_info;
        }
        last_ad = &ad;
    }

    if (last_ad) {
        last_ad->game_info.pNext = nullptr;
    }
    *out_game_list = &g_snp_context.status_ad.game_info;

    return true;
}

static BOOL __stdcall spi_unlock_game_list(GameInfo* game_list, DWORD*) {
    // Called by storm after it is done reading the games list to unlock the mutex
    g_advertisement_mutex.unlock();

    g_crown_link->request_advertisements();

    return true;
}

static void create_ad(
    AdFile& ad_file, const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data,
    DWORD user_data_size
) {
    const auto& snp_config = SnpConfig::instance();
    memset(&ad_file, 0, sizeof(ad_file));
    ad_file.crownlink_mode = snp_config.mode;
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

void create_status_ad() {
    std::lock_guard lock{g_advertisement_mutex};
    char user_data[32] = {12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                          0,  0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0};
    create_ad(
        g_snp_context.status_ad, "", ",33,,3,,1e,,1,cb2edaab,5,,Server\rStatus\r", 12, &user_data, sizeof(user_data)
    );
    g_snp_context.status_ad.game_info.game_index = 1;
    g_snp_context.status_ad.game_info.pNext = nullptr;
}

void update_status_ad() {
    const auto& snp_config = SnpConfig::instance();
    std::string output{};
    // "start of text" character changes to a heading font and makes sure the status is at the top of the list
    if (g_snp_context.status_string.empty()) {
        if (!snp_config.lobby_password.empty()) {
            output += std::format("{}Private ", char(ColorByte::Blue));
        }
        if (snp_config.mode == CrownLinkMode::DBCL) {
            if (output.empty()) {
                output += std::format("{}", char(ColorByte::Blue));
            }
            output += "DBC ";
        }
        if (!output.empty()) {
            output += "Mode";
        }
    } else {
        output = std::format("{}{}", char(ColorByte::LightGreen), g_snp_context.status_string);
        // intentionally not using a "start of text" character here so the ad is green
    }
    if (output.empty()) {
        g_snp_context.status_ad.game_info.game_state = 12;  // hide the ad, games list doesn't show games "in progress"
        return;
    }
    g_snp_context.status_ad.game_info.game_state = 0;  // show the ad
    strcpy_s(
        g_snp_context.status_ad.game_info.game_name, sizeof(g_snp_context.status_ad.game_info.game_name), output.c_str()
    );
}

void set_status_ad(const std::string& status) {
    g_snp_context.status_string = status;
}

void clear_status_ad() {
    g_snp_context.status_string = "";
}

static BOOL __stdcall spi_start_advertising_ladder_game(
    char* game_name, char* game_password, char* game_stat_string, DWORD game_state, DWORD elapsed_time, DWORD game_type,
    int, int, void* user_data, DWORD user_data_size
) {
    // called by storm when the user creates a new lobby and also when the lobby info changes (e.g. player joins/leaves)
    std::lock_guard lock{g_advertisement_mutex};
    spdlog::info("game_stat_string: {}", game_stat_string);
    const auto& snp_config = SnpConfig::instance();
    create_ad(g_snp_context.hosted_game, game_name, game_stat_string, game_state, user_data, user_data_size);
    g_snp_context.hosted_game.crownlink_mode = snp_config.mode;
    g_crown_link->start_advertising(g_snp_context.hosted_game);
    spdlog::info("Started advertising");
    return true;
}

static BOOL __stdcall spi_stop_advertising_game() {
    // called by storm when the host cancels their lobby or the game is over

    std::lock_guard lock{g_advertisement_mutex};
    g_crown_link->stop_advertising();
    spdlog::info("Stopped advertising");
    return true;
}

static BOOL __stdcall spi_get_game_info(DWORD index, char* game_name, int buffer_size, GameInfo* out_game) {
    // called by storm when the user selects a lobby to join from the games list
    // if we return false the user will immediately see a "couldn't join game" message

    std::lock_guard lock{g_advertisement_mutex};
    spdlog::trace("getting game info for index {}", index);

    ProviderInfo* current_provider = nullptr;
    for (ProviderInfo provider : g_providers) {
        if (provider.provider_id == g_provider_id) {
            current_provider = &provider;
        }
    }

    for (auto& ad : g_snp_context.lobbies) {
        spdlog::trace(
            "checking game: {} with index {}, version {}", ad.game_info.game_description, ad.game_info.game_index,
            ad.game_info.version_id
        );
        const auto& snp_config = SnpConfig::instance();
        if (ad.game_info.game_index == index) {
            *out_game = ad.game_info;
            spdlog::trace("found game with description {}", ad.game_info.game_description);
            if (current_provider && ad.crownlink_mode == CrownLinkMode::CLNK) {
                current_provider->caps.turns_per_second = 8;
            } else if (current_provider) {
                current_provider->caps.turns_per_second = 4;
            } else {
                // couldn't find current provider, this should never happen
                return false;
            }
            return true;
        }
    }

    return false;
}

//===========================================================================
//
// P2P Messaging
//
//===========================================================================

static BOOL __stdcall spi_send(DWORD address_count, NetAddress** out_address_list, char* data, DWORD size) {
    if (!address_count) {
        spdlog::error("spiSend called with no addresses");
        return true;
    }

    if (address_count > 1) {
        spdlog::error("multicast attempted");  // BW engine doesn't seem to ever use multicast
    }

    const NetAddress& peer = out_address_list[0][0];
    GamePacketData* packet = reinterpret_cast<GamePacketData*>(data);
    spdlog::trace(
        "[{}] Send: {} {:pa}", peer, to_string(packet->header),
        spdlog::to_hex(
            std::begin(packet->payload), std::begin(packet->payload) + packet->header.size - sizeof(GamePacketHeader)
        )
    );

    return g_crown_link->send(peer, data, size);
}

static BOOL __stdcall spi_receive(NetAddress** peer, char** out_data, DWORD* out_size) {
    // when g_receive_event is set storm will repeatedly call this until it returns false, reading one packet each time
    // until the queue is empty

    *peer = nullptr;
    *out_data = nullptr;
    *out_size = 0;

    GamePacket* loan = new GamePacket{};

    while (true) {
        if (!g_crown_link->receive_queue().try_dequeue(*loan)) {
            delete loan;
            return false;
        }
        auto payload_size = loan->data.header.size - sizeof(GamePacketHeader);
        spdlog::trace(
            "[{}] Recv: {} {:pa}", loan->sender, to_string(loan->data.header),
            spdlog::to_hex(std::begin(loan->data.payload), std::begin(loan->data.payload) + payload_size)
        );

        if (get_tick_count() > loan->timestamp + 2) {
            continue;
        }

        *peer = &loan->sender;
        *out_data = (char*)&loan->data;
        *out_size = loan->size;

        break;
    }
    return true;
}
// TODO - template this out, helper functions, use a queue to send to a separate thread
void packet_parser(const GamePacket* game_packet) {
    auto payload_size = game_packet->data.header.size - sizeof(GamePacketHeader);
    switch (game_packet->data.header.sub_type) {
        case GamePacketSubType::PlayerInfo: {
            SystemPlayerJoin_PlayerInfo player_info{};
            memcpy(
                &player_info, game_packet->data.payload,
                payload_size < sizeof(SystemPlayerJoin_PlayerInfo) ? payload_size : sizeof(SystemPlayerJoin_PlayerInfo)
            );
            if (player_info.address.is_zero()) {
                player_info.address = game_packet->sender;
                spdlog::trace("address was zero, copying from game info");
            }
            spdlog::trace(
                "Player introduction received, address: {}, player id: {}, host: {}, name {}", player_info.address,
                player_info.player_id, player_info.gameowner, player_info.name_description
            );
        }
    }
}

static BOOL __stdcall spi_free(NetAddress* loan, char* data, DWORD size) {
    // called after storm is done with the packet to free its memory
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
        return false;
    }

    *out_result = (address1 == address2);
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
    return false;
}

bool snp::try_create_game(u32* playerid) {
    char name[25] = "Jesse";
    char desc[25] = "Description";
    PlayerData player_data{sizeof(PlayerData), name, desc};
    
    CreateInfo create_data{sizeof(CreateInfo), 'BNET', 256, 1}; // flags: 1 = private games allowed

    if (!GetModuleFileNameA(0, bw_starcraft_exe_path, 260)) {
        // todo result handling
    }
    const char* mapfile = "C:\\Cosmonarchy\\Starcraft\\Maps\\CMBW-Root\\CMBW-maps\\melee\\1v1\\_event\\(2)Demilune 1.3.scx";
    const char* mapname = "Demilune";
    const char* mapsdir = "C:\\Cosmonarchy\\Starcraft\\Maps\\CMBW-Root\\CMBW-maps\\melee\\1v1\\_event\\";
    //const char* mapsdir = "C:\\Cosmonarchy\\Starcraft\\Maps\\replays\\a\\";
    strncpy(bw_maps_directory, mapsdir, 260); 
    //strncpy(bw_current_map_filename, mapfile, 260);
    //strncpy(bw_current_map_name, mapname, 32);

    // build directory listing linked list
    list_dir(mapsdir);

    auto first_entry = *(DirEntryMap**)0x0051a27c;
    /////////////////////////////

    // make sure game name & mapsdir aren't null
    // loop over the list to find selected entry
    // verify we found it
    // check selection type to make sure it's a map

    /////////////////////////////

    map_load_info(first_entry);

    auto s = create_game("Jesse", "", 0x01001e, 6, mapsdir, first_entry);
    return 1;

    // 
    DirEntryMap demilune_map{nullptr, nullptr, "(2)Demilune 1.3.scx",
    };
    demilune_map.selection_type = 0x24;
    strncpy(demilune_map.full_path, mapfile, 260);
    strncpy(demilune_map.full_filename, "(2)Demilune 1.3.scx", 260);




    //GameData game_data{0, "Jesse", 00, 112, 128, 1, 8, 0, 0, 30, 1, 3408845483, 7, 0, 0, "Jesse", "Demilune", 30, 1,
    //    0,0,0,0,1,2,0,1,3,1,0,0,0,0,0,0
    //};

    // should be ready
 //   map_load_info(&demilune_map);

 //   char outstr[32]{};

 //   auto mps = map_prep(mapfile, outstr, 0);
 //   spdlog::info("Map prep success: {}, outstr: {}", mps, outstr);

 //   GameData create_game_data{};
 //   ZeroMemory(&create_game_data, sizeof(GameData));
 //   strncpy(create_game_data.game_name, "Jesse", 24);
 //   strncpy(create_game_data.map_title, demilune_map.map_name, 25);
 //   create_game_data.map_height_tiles = demilune_map.map_height_tiles;
 //   create_game_data.map_width_tiles = demilune_map.map_width_tiles;
 //   create_game_data.max_player_count = demilune_map.max_players;
 //   create_game_data.g_game_type = 0x02;
 //   //create_game_data.game_subtype = 0x1e;
 //   create_game_data.game_state = demilune_map.game_state;
 //   create_game_data.tileset = *(u16*)0x0057f1dc;
 //   create_game_data.active_player_count = 1;

 //   auto result = create_ladder_game(&create_game_data, "");

 //   if (result != 0) {
 //   	spdlog::info("Error creating game: {}", result);
	//}

 //   return result;
}

bool snp::join_game(u32 index, u32* playerid) {
    while (true) {
        if (g_snp_context.lobbies.size()) {
            auto ad = g_snp_context.lobbies[0];
            TCHAR gamename[128];
            GameInfo gi{};
            spi_get_game_info(ad.game_info.game_index, gamename, 0, &gi);
            DWORD id = 3;
            spdlog::info("game owner: {}", gi.host);

            auto juice_state = g_crown_link->juice_manager().lobby_agent_state(ad);
            while (juice_state != JUICE_STATE_COMPLETED) {
                spdlog::info("waiting for p2p completion, state: {}", to_string(juice_state));
                std::this_thread::sleep_for(1s);
                juice_state = g_crown_link->juice_manager().lobby_agent_state(ad);
            }
            const char* name = "Jesse";

            // todo - need to parse description string to get game type
            *bw_game_type = 0x1e;
            *bw_game_subtype = 1;

            auto res = SNetJoinGame(
                ad.game_info.game_index, (const char*)0, (const char*)0, name, (const char*)0, (u32*)playerid
            );

            return res;
        }
        std::this_thread::sleep_for(1s);
        g_crown_link->request_advertisements();
    }

}


static BOOL __stdcall spi_select_game(
    DWORD flags, ProgramInfo* client_info, PlayerData* user_info, InterfaceData* interface_data, VersionInfo* module_info, u32* playerid
) {
    
    
    for (ProviderInfo provider : g_providers) {
        spdlog::info("Provider ID {}", provider.provider_id);
    }
    
    
    
    
    
    
    
    spdlog::info("called spi_select, flags: {}", flags);
    g_crown_link->request_advertisements();
    

    g_snp_context.player_id = *playerid;
    if (!interface_data) {
        return false;
    }
    HWND main_game_window = interface_data->parent_window;


    return join_game(0, playerid);



    strncpy(bw_player_name, "Jesse", 6);


    CreateInfo game_create_info{sizeof(CreateInfo), 'BNET', 8, 1}; // SNET_CF_ALLOWPRIVATEGAMES
    PlayerData player_data{16, bw_player_name, (char*)"Desc", 0};
    UIParams ui_params{flags, client_info, &player_data, interface_data, module_info, playerid};

    SetActiveWindow(interface_data->parent_window);

    //g_snp_context.ui_params.interface_data->pfnBattleErrorDialog(
    //    g_snp_context.ui_params.interface_data->parent_window, "this is the text", "caption!", 1
    //);

    return try_create_game(playerid);



    //if (!res) {
    //    auto err = SErrGetLastError();
    //    char buffer[255];
    //    SErrGetErrorStr(err, buffer, 254);
    //    spdlog::info("Error: {} str: {}", err, buffer);
    //}
    //return res;

}

static BOOL __stdcall spi_get_local_player_name(
    char* name_buffer, DWORD name_buffer_size, char* desc_buffer, DWORD desc_buffer_size
) {
    // temporary, for testing
    // todo
    if (name_buffer_size > 0) {
        strncpy(name_buffer, "Jesse", 6); 
    }
    if (desc_buffer_size > 0) {
        strncpy(desc_buffer, "Desc", 5);
    }
    
    return true;
}

static BOOL __stdcall spiReportGameResult(u32 a, u32 b, u32 c, u32 d, u32 e) {
    spdlog::info("spiReportGameResult called, params: {}, {}, {}, {}, {}", a, b, c, d, e);
	return true;
}

static BOOL __stdcall spi_send_external_message(int, int, int, int, int) {
    return false;
}

static BOOL __stdcall spi_league_get_name(char* data, DWORD size) {
    spdlog::info("spi_league_get_name called");

    return true;
}

snp::NetFunctions g_spi_functions = {
    sizeof(snp::NetFunctions),
    &snp::spi_compare_net_addresses,
    &snp::spi_destroy,
    &snp::spi_free,
    &snp::spi_free_external_message,
    &snp::spi_get_game_info,
    &snp::spi_get_performance_data,
    &snp::spi_initialize,
    &snp::spi_initialize_device,
    &snp::spi_lock_device_list,
    &snp::spi_lock_game_list,
    &snp::spi_receive,
    &snp::spi_receive_external_message,
    &snp::spi_select_game,
    &snp::spi_send,
    &snp::spi_send_external_message,
    &snp::spi_start_advertising_ladder_game,
    &snp::spi_stop_advertising_game,
    &snp::spi_unlock_device_list,
    &snp::spi_unlock_game_list,
    &snp::spi_get_local_player_name,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &snp::spi_league_get_name
};

}  // namespace snp
