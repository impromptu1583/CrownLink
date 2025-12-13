#include <list>

#include "SNPModule.h"
#include "CrownLink.h"

namespace snp {

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
    ClientInfo* client_info, UserInfo* user_info, BattleInfo* callbacks, ModuleInfo* module_data, HANDLE event
) {
    // called by storm when the CrownLink connection mode is selected from the multiplayer menu
    auto& context = SNPContext::instance();
    context.game_app_info = *client_info;
    g_receive_event = event;
    const auto& snp_config = SnpConfig::instance();
    init_logging();
    create_status_ad();
    set_status_ad("Initializing");

    try {
        g_crown_link = std::make_unique<CrownLink>();
    } catch (const std::exception& e) {
        spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
        return false;
    }

    spdlog::info(
        "Crownlink Initializing, turns_per_second: {}, game version: {}", to_string(context.turns_per_second),
        context.game_app_info.version_id
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

    auto& context = SNPContext::instance();
    for (AdFile& known_lobby : context.lobbies) {
        auto it = std::ranges::find_if(updated_list.begin(), updated_list.end(), [&known_lobby](const AdFile& incoming_ad) {
            return known_lobby.is_same_owner(incoming_ad);
        });
        if (it != updated_list.end()) {
            known_lobby = *it;
            it->mark_for_removal = true;
        } else {
            known_lobby.mark_for_removal = true;
        }
    }
    context.lobbies.erase(
        std::remove_if(
            context.lobbies.begin(), context.lobbies.end(),
            [](const AdFile& ad) {
                return ad.mark_for_removal;
            }
        ),
        context.lobbies.end()
    );

    std::ranges::copy_if(updated_list, std::back_inserter(context.lobbies), [](const AdFile& incoming_ad) {
        return !incoming_ad.mark_for_removal;
    });

    const auto last_updated = get_tick_count();
    u32 index = 1;
    for (AdFile& known_lobby : context.lobbies) {
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

    // Called by storm once per second when on the games list screen
static BOOL __stdcall spi_lock_game_list(DWORD category_bits, DWORD category_mask, AdFile** out_game_list) {
    g_crown_link->request_advertisements();

    // Storm will unlock when it's done with the data by calling spi_unlock_game_list()
    g_advertisement_mutex.lock();

    auto& context = SNPContext::instance();

    if (!context.status_ad_used && context.lobbies.empty()) {
        // If we return false Storm won't call spi_unlock_game_list() to unlock the mutex
        g_advertisement_mutex.unlock();
        return false;
    }

    const auto& snp_config = SnpConfig::instance();

    AdFile* last_ad = nullptr;

    update_status_ad();
    last_ad = &context.status_ad;

    std::erase_if(context.lobbies, [](const AdFile& lobby) {
        return get_tick_count() - lobby.game_info.host_last_time > 2;
    });

    for (AdFile& ad : context.lobbies) {
        bool joinable = true;
        bool has_text_prefix = false;
        std::stringstream ss;

        if (context.game_app_info.version_id != ad.game_info.version_id) {
            joinable = false;
            has_text_prefix = true;
            ss << ColorByte::Red << "[!Ver]";
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
                            static constexpr char SadEmoji = 131;
                            ss << std::format("[Radmin {}]", SadEmoji);
                        } break;
                        default: {
                            has_text_prefix = true;
                            static constexpr char DoubleArrow = 187;
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
    if (context.status_ad_used) {
        *out_game_list = &context.status_ad;
        return true;
    } else if (context.lobbies.size()) {
        *out_game_list = &context.lobbies[0];
        return true;
    }
    *out_game_list = nullptr;
}

static BOOL __stdcall spi_unlock_game_list(AdFile* game_list, DWORD* list_cout) {
    // Called by storm after it is done reading the games list to unlock the mutex
    g_advertisement_mutex.unlock();
    return true;
}

static void create_ad(
    AdFile& ad_file, const char* game_name, const char* game_stat_string, DWORD game_state, void* user_data,
    DWORD user_data_size
) {
    auto& context = SNPContext::instance();
    ad_file = {};
    ad_file.crownlink_mode = context.turns_per_second;
    auto& game_info = ad_file.game_info;
    strcpy_s(game_info.game_name, sizeof(game_info.game_name), game_name);
    strcpy_s(game_info.game_description, sizeof(game_info.game_description), game_stat_string);
    game_info.game_state = game_state;
    game_info.program_id = context.game_app_info.program_id;
    game_info.version_id = context.game_app_info.version_id;
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
    auto& context = SNPContext::instance();
    create_ad(context.status_ad, "", ",33,,3,,1e,,1,cb2edaab,5,,Server\rStatus\r", 12, &user_data, sizeof(user_data)
    );
    context.status_ad.game_info.game_index = 1;
    context.status_ad.game_info.pNext = nullptr;
}

void update_status_ad() {
    const auto& snp_config = SnpConfig::instance();
    auto& context = SNPContext::instance();
    std::string output{};
    // "start of text" character changes to a heading font and makes sure the status is at the top of the list
    if (context.status_string.empty()) {
        if (!snp_config.lobby_password.empty()) {
            output += std::format("{}Private ", char(ColorByte::Blue));
        }
        if (context.turns_per_second == TurnsPerSecond::CLDB || context.turns_per_second == TurnsPerSecond::UltraLow) {
            if (output.empty()) {
                output += std::format("{}", char(ColorByte::Blue));
            }
            output += "DBC ";
        }
        if (!output.empty()) {
            output += "Mode";
        }
    } else {
        output = std::format("{}{}", char(ColorByte::LightGreen), context.status_string);
        // intentionally not using a "start of text" character here so the ad is green
    }
    if (output.empty()) {
        context.status_ad.game_info.game_state = 12;  // hide the ad, games list doesn't show games "in progress"
        return;
    }
    context.status_ad.game_info.game_state = 0;  // show the ad
    strcpy_s(context.status_ad.game_info.game_name, sizeof(context.status_ad.game_info.game_name), output.c_str()
    );
}

void set_status_ad(const std::string& status) {
    auto& context = SNPContext::instance();
    context.status_string = status;
}

void clear_status_ad() {
    auto& context = SNPContext::instance();
    context.status_string = "";
}

static BOOL __stdcall spi_start_advertising_ladder_game(
    const char* game_name, const char* game_password, const char* game_stat_string, DWORD game_state, DWORD elapsed_time, DWORD game_type,
    DWORD, DWORD, void* user_data, DWORD user_data_size
) {
    // called by storm when the user creates a new lobby and also when the lobby info changes (e.g. player joins/leaves)
    std::lock_guard lock{g_advertisement_mutex};

    auto& context = SNPContext::instance();
    create_ad(context.hosted_game, game_name, game_stat_string, game_state, user_data, user_data_size);
    context.hosted_game.crownlink_mode = context.turns_per_second;
    g_crown_link->start_advertising(context.hosted_game);
    spdlog::info("Started advertising");

    set_turns_per_second(context.turns_per_second);

    return true;
}

static BOOL __stdcall spi_stop_advertising_game() {
    // called by storm when the host cancels their lobby or the game is over

    std::lock_guard lock{g_advertisement_mutex};
    g_crown_link->stop_advertising();
    spdlog::info("Stopped advertising");
    return true;
}

static BOOL __stdcall spi_get_game_info(DWORD index, const char* game_name, const char* password, GameInfo* out_game) {
    // called by storm when the user selects a lobby to join from the games list
    // if we return false the user will immediately see a "couldn't join game" message
    std::lock_guard lock{g_advertisement_mutex};

    auto& context = SNPContext::instance();
    for (auto& ad : context.lobbies) {
        if (ad.game_info.game_index == index) {
            *out_game = ad.game_info;
            if (ad.crownlink_mode == TurnsPerSecond::CNLK) {
                g_current_provider->caps.turns_per_second = 8;
            } else if (ad.crownlink_mode == TurnsPerSecond::CLDB) {
                g_current_provider->caps.turns_per_second = 4;
            } else {
                g_current_provider->caps.turns_per_second = ad.crownlink_mode;
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

static BOOL __stdcall spi_receive(NetAddress** peer, GamePacketData** out_data, DWORD* out_size) {
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
        *out_data = &loan->data;
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

bool set_turns_per_second(TurnsPerSecond turns_per_second) {
    if (is_valid(turns_per_second)) {
        auto& context = SNPContext::instance();
        context.turns_per_second = turns_per_second;
        if (turns_per_second == TurnsPerSecond::CNLK) {
            g_current_provider->caps.turns_per_second = 8;
        } else if (turns_per_second == TurnsPerSecond::CLDB) {
            g_current_provider->caps.turns_per_second = 4;
        } else {
            g_current_provider->caps.turns_per_second = turns_per_second;
        }
        return true;
    }
    return false;
}

static BOOL __stdcall spi_free(NetAddress* loan, char* data, DWORD size) {
    // called after storm is done with the packet to free its memory
    if (loan) {
        delete loan;
    }
    return true;
}

static BOOL __stdcall spi_compare_net_addresses(const NetAddress* address1, const NetAddress* address2, DWORD* out_result) {
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

static BOOL __stdcall spi_get_performance_data(DWORD type, DWORD* out_result, DWORD, DWORD) {
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

static BOOL __stdcall spi_select_game(
    DWORD flags, ClientInfo* client_info, UserInfo* user_info, BattleInfo* callbacks, ModuleInfo* module_info, DWORD* player_id
) {
    // This is used for battle.net instead of the games list.
    // BW calls this function then waits for battle.net to start a multiplayer game using the appropriate storm ordinal
    return false;
}

static BOOL __stdcall spi_send_external_message(
    char* destination, DWORD message_size, char* blank1, char* blank2, char* message
) {
    return false;
}

static BOOL __stdcall spi_league_get_name(char* data, DWORD size) {
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
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    &snp::spi_league_get_name
};

}  // namespace snp
