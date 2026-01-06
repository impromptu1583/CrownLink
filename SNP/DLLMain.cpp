#include <juice.h>

#include "AdvertisementManager.h"
#include "Config.h"
#include "CrowServeManager.h"
#include "Globals.h"
#include "Logger.h"
#include "SNPModule.h"
#include "BWInteractions.h"

constexpr auto CLNK_ID = 0;
constexpr auto CLDB_ID = 1;

BOOL WINAPI SnpQuery(
    u32 index, u32* out_network_code, char** out_network_name, char** out_network_description, Caps** out_caps
) {
    if (out_network_code && out_network_name && out_network_description && out_caps) {
        switch (index) {
            case CLNK_ID: {
                g_network_info.caps.turns_per_second = TurnsPerSecond::CNLK;
                *out_network_code = g_network_info.id;
                *out_network_name = g_network_info.name;
                *out_network_description = g_network_info.description;
                *out_caps = &g_network_info.caps;
                return true;
            }
            case CLDB_ID: {
                g_network_info.caps.turns_per_second = TurnsPerSecond::CLDB;
                *out_network_code = g_network_info.id;
                *out_network_name = g_network_info.name;
                *out_network_description = g_network_info.description;
                *out_caps = &g_network_info.caps;
                return true;
            }
        }
    }
    return false;
}

BOOL WINAPI SnpBind(u32 index, snp::NetFunctions** out_funcs) {
    if (out_funcs) {
        switch (index) {
            case CLNK_ID: {
                snp::set_snp_turns_per_second(TurnsPerSecond::CNLK);
                *out_funcs = &snp::g_spi_functions;
                return true;
            }
            case CLDB_ID: {
                snp::set_snp_turns_per_second(TurnsPerSecond::CLDB);
                *out_funcs = &snp::g_spi_functions;
                return true;
            }
        }
    }
    return false;
}

u32 WINAPI version() {
    return CL_VERSION_NUMBER;
}

// TODO: improve this - should use c-style user pointer for consistency
BOOL WINAPI register_status_callback(CrowServe::StatusCallback callback, void* user_data) {
    if (!g_context) return false;
    g_context->crowserve().socket().set_status_callback(callback, user_data);

    return true;
}

void WINAPI iterate_lobbies(void (*callback)(AdFile*, ConnectionState peer_state, void*), void* user_data) {
    AdvertisementManager::instance().iterate_lobby_list(callback, user_data);
}

BOOL WINAPI set_turns_per_second(TurnsPerSecond turns_per_second) {
    return snp::set_snp_turns_per_second(turns_per_second);
}

TurnsPerSecond WINAPI get_turns_per_second() {
    return snp::get_snp_turns_per_second();
}

BOOL WINAPI set_password(const char* password) {
    auto& snp_config = SnpConfig::instance();
    snp_config.lobby_password = password;
    AdvertisementManager::instance().set_lobby_password(password);
    return true;
}

void WINAPI get_password(char* output, u32 output_size) {
    AdvertisementManager::instance().get_lobby_password(output, output_size);
}

CrowServe::SocketState WINAPI get_status() {
    if (!g_context) return CrowServe::SocketState::Disconnected;
    return g_context->crowserve().socket().state();
}

void WINAPI set_status_lobby(bool enable) {
    AdvertisementManager::instance().use_status_add(enable);
}

void WINAPI set_map_name_edit(bool enable) {
    AdvertisementManager::instance().edit_game_name(enable);
}

static void juice_logger(juice_log_level_t log_level, const char* message) {
    switch (log_level) {
        case JUICE_LOG_LEVEL_VERBOSE:
            spdlog::trace("{}", message);
            break;
        case JUICE_LOG_LEVEL_DEBUG:
            spdlog::trace("{}", message);
            break;
        case JUICE_LOG_LEVEL_WARN:
            spdlog::warn("{}", message);
            break;
        case JUICE_LOG_LEVEL_INFO:
            spdlog::info("{}", message);
            break;
        case JUICE_LOG_LEVEL_ERROR:
            spdlog::error("{}", message);
            break;
        case JUICE_LOG_LEVEL_FATAL:
            spdlog::error("{}", message);
            break;
    }
}

static void dll_start() {
    WSADATA wsaData{};
    WORD wVersionRequested = MAKEWORD(2, 2);
    if (auto error_code = WSAStartup(wVersionRequested, &wsaData); error_code != S_OK) {
        spdlog::critical("WSAStartup failed with error {}", error_code);
    }

    juice_set_log_handler(juice_logger);
    juice_set_log_level(JUICE_LOG_LEVEL_INFO);
}

static void dll_exit() {
    WSACleanup();
}

BOOL WINAPI DllMain(HINSTANCE instance, u32 reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            dll_start();
            std::atexit(dll_exit);
        } break;
    }
    return true;
}
