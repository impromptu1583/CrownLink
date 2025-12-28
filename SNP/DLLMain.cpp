#include "Logger.h"
#include "SNPModule.h"
#include "AdvertisementManager.h"

constexpr auto CLNK_ID = 0;
constexpr auto CLDB_ID = 1;

BOOL WINAPI SnpQuery(
    DWORD index, DWORD* out_network_code, char** out_network_name, char** out_network_description, Caps** out_caps
) {
    if (out_network_code && out_network_name && out_network_description && out_caps) {
        switch (index) {
            case CLNK_ID: {
                *out_network_code = snp::g_network_info.id;
                *out_network_name = snp::g_network_info.name;
                *out_network_description = snp::g_network_info.description;
                *out_caps = &snp::g_network_info.caps;
                return true;
            }
            case CLDB_ID: {
                snp::g_network_info.caps.turns_per_second = 4;
                *out_network_code = snp::g_network_info.id;
                *out_network_name = snp::g_network_info.name;
                *out_network_description = snp::g_network_info.description;
                *out_caps = &snp::g_network_info.caps;
                return true;
            }
        }
    }
    return false;
}

BOOL WINAPI SnpBind(DWORD index, snp::NetFunctions** out_funcs) {
    if (out_funcs) {
        switch (index) {
            case CLNK_ID: {
                snp::set_turns_per_second(TurnsPerSecond::Standard);
                *out_funcs = &snp::g_spi_functions;
                return true;
            }
            case CLDB_ID: {
                snp::set_turns_per_second(TurnsPerSecond::UltraLow);
                *out_funcs = &snp::g_spi_functions;
                return true;
            }
        }
    }
    return false;
}

u32 WINAPI Version() {
    return CL_VERSION_NUMBER;
}

BOOL WINAPI RegisterStatusCallback(CrowServe::StatusCallback callback, bool use_status_lobby, bool edit_name) {
    if (!g_crowserve) return false;
    g_crowserve->socket().register_status_callback(callback);
    AdvertisementManager::instance().use_status_add(use_status_lobby);
    AdvertisementManager::instance().edit_game_name(edit_name);

    return true;
}

BOOL WINAPI SetTurnsPerSecond(TurnsPerSecond turns_per_second) {
    return snp::set_turns_per_second(turns_per_second);
}

TurnsPerSecond WINAPI GetTurnsPerSecond() {
    return snp::get_turns_per_second();
}

BOOL WINAPI SetLobbyPassword(const char* password) {
    auto& snp_config = SnpConfig::instance();
    snp_config.lobby_password = password;
    AdvertisementManager::instance().set_lobby_password(password);
    return true;
}

void WINAPI GetLobbyPassword(char* output, u32 output_size) {
    AdvertisementManager::instance().get_lobby_password(output, output_size);
}

CrowServe::SocketState WINAPI GetStatus() {
    if (!g_crowserve) return CrowServe::SocketState::Disconnected;
    return g_crowserve->socket().state();
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

BOOL WINAPI DllMain(HINSTANCE instance, DWORD reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            dll_start();
            std::atexit(dll_exit);
        } break;
    }
    return true;
}
