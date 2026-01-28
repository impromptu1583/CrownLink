#include "SNPModule.h"
#include <list>
#include <utility>
#include "AdvertisementManager.h"
#include "BWInteractions.h"
#include "Config.h"
#include "CrowServeManager.h"
#include "Globals.h"
#include "JuiceManager.h"

namespace snp {

static void init_logging() {
    const auto& snp_config = SnpConfig::instance();
    spdlog::init_thread_pool(8192, 1);

    const auto log_filename = (g_starcraft_dir / "crownlink_logs" / "CrownLink.txt").generic_string();
    auto standard_sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(log_filename, 2, 30);
    standard_sink->set_level(spdlog::level::debug);

    const auto trace_filename = (g_starcraft_dir / "crownlink_logs" / "CLTrace.txt").generic_string();
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

static b32 __stdcall spi_initialize(
    ClientInfo* client_info, UserInfo* user_info, BattleInfo* callbacks, ModuleInfo* module_data, handle event
) {
    // called by storm when the CrownLink connection mode is selected from the multiplayer menu
    const auto& snp_config = SnpConfig::instance();
    init_logging();

    try {
        g_context = std::make_unique<Context>();
        g_context->set_receive_event(event);
        g_context->set_client_info(client_info);
    } catch (const std::exception& e) {
        spdlog::error("Unhandled error {} in {}", e.what(), __FUNCSIG__);
        return false;
    }

    AdvertisementManager::instance().set_status_ad("Initializing");
    AdvertisementManager::instance().set_lobby_password(snp_config.lobby_password.c_str());

    spdlog::info(
        "Crownlink Initializing, turns_per_second: {}, game version: {}",
        std::to_underlying(g_network_info.caps.turns_per_second),
        g_context->client_info().version_id
    );
    
    return true;
}

static b32 __stdcall spi_destroy() {
    // called by storm when exiting out of the multiplayer lobbies / game finished screen and back to the multiplayer
    // connnection types menu
    try {
        g_context.reset();
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

// Called by storm once per second when on the games list screen
static b32 __stdcall spi_lock_game_list(u32 category_bits, u32 category_mask, AdFile** out_game_list) {
    return AdvertisementManager::instance().lock_game_list(category_bits, category_mask, out_game_list);
}

static b32 __stdcall spi_unlock_game_list(AdFile* game_list, u32* list_cout) {
    // Called by storm after it is done reading the games list to unlock the mutex
    return AdvertisementManager::instance().unlock_game_list();
}

static b32 __stdcall spi_start_advertising_ladder_game(
    const char* game_name, const char* game_password, const char* game_stat_string, u32 game_state, u32 elapsed_time, u32 game_type,
    u32, u32, void* user_data, u32 user_data_size
) {
    // called by storm when the user creates a new lobby and also when the lobby info changes (e.g. player joins/leaves)
    AdvertisementManager::instance().start_advertising(game_name, game_stat_string, game_state, user_data, user_data_size);
    set_provider_turns_per_second(g_network_info.caps.turns_per_second);
    spdlog::info("Started advertising");

    return true;
}

static b32 __stdcall spi_stop_advertising_game() {
    // called by storm when the host cancels their lobby or the game is over
    AdvertisementManager::instance().stop_advertising();
    spdlog::info("Stopped advertising");
    return true;
}

static b32 __stdcall spi_get_game_info(u32 index, const char* game_name, const char* password, GameInfo* output) {
    // called by storm when the user selects a lobby to join from the games list
    // if we return false the user will immediately see a "couldn't join game" message
    auto maybe_ad = AdvertisementManager::instance().get_ad_by_index(index);
    if (maybe_ad) {
        auto& adfile = maybe_ad->get();
        *output = adfile.game_info;
        set_provider_turns_per_second(adfile.turns_per_second);
        return true;
    }
    return false;
}

//===========================================================================
//
// P2P Messaging
//
//===========================================================================

static b32 __stdcall spi_send(u32 address_count, NetAddress** out_address_list, char* data, u32 size) {
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

    return g_context->juice_manager().send_p2p(peer, data, size);
}

static b32 __stdcall spi_receive(NetAddress** peer, GamePacketData** out_data, u32* out_size) {
    // when g_receive_event is set storm will repeatedly call this until it returns false, reading one packet each time
    // until the queue is empty

    *peer = nullptr;
    *out_data = nullptr;
    *out_size = 0;

    GamePacket* loan = new GamePacket{};

    while (true) {
        if (!g_context || !g_context->receive_queue().try_dequeue(*loan)) {
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
            if (is_zero(player_info.address)) {
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

bool set_snp_turns_per_second(TurnsPerSecond turns_per_second) {
    if (is_valid(turns_per_second)) {
        g_network_info.caps.turns_per_second = turns_per_second;
        set_provider_turns_per_second(turns_per_second);
        return true;
    }
    return false;
}

TurnsPerSecond get_snp_turns_per_second() {
    return g_network_info.caps.turns_per_second;
}

static b32 __stdcall spi_free(GamePacket* loan, char* data, u32 size) {
    // called after storm is done with the packet to free its memory
    if (loan) {
        delete loan;
    }
    return true;
}

static b32 __stdcall spi_compare_net_addresses(const NetAddress* address1, const NetAddress* address2, u32* out_result) {
    if (out_result) {
        *out_result = 0;
    }
    if (!address1 || !address2 || !out_result) {
        return false;
    }

    *out_result = (address1 == address2);
    return true;
}

static b32 __stdcall spi_lock_device_list(u32* out_device_list) {
    *out_device_list = 0;
    return true;
}

static b32 __stdcall spi_unlock_device_list(void*) {
    return true;
}

static b32 __stdcall spi_free_external_message(NetAddress* address, char* data, u32 size) {
    return false;
}

static b32 __stdcall spi_get_performance_data(u32 type, u32* out_result, u32, u32) {
    return false;
}

static b32 __stdcall spi_initialize_device(int, void*, void*, u32*, void*) {
    return false;
}

static b32 __stdcall spi_receive_external_message(NetAddress** out_address, char** out_data, u32* out_size) {
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

static b32 __stdcall spi_select_game(
    u32 flags, ClientInfo* client_info, UserInfo* user_info, BattleInfo* callbacks, ModuleInfo* module_info, u32* player_id
) {
    // This is used for battle.net instead of the games list.
    // BW calls this function then waits for battle.net to start a multiplayer game using the appropriate storm ordinal
    return false;
}

static b32 __stdcall spi_send_external_message(
    char* destination, u32 message_size, char* blank1, char* blank2, char* message
) {
    return false;
}

static b32 __stdcall spi_league_get_name(char* data, u32 size) {
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
