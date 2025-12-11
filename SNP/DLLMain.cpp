#include <winsock2.h>

#include "SNPModule.h"
#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#include "CrownLink.h"
#define CLNK_ID 0
#define DBCL_ID 1

BOOL WINAPI SnpQuery(
    DWORD index, DWORD* out_network_code, char** out_network_name, char** out_network_description, Caps** out_caps
) {
    if (out_network_code && out_network_name && out_network_description && out_caps) {
        switch (index) {
            case CLNK_ID: {
                *out_network_code = g_network_info.id;
                *out_network_name = g_network_info.name;
                *out_network_description = g_network_info.description;
                *out_caps = &g_network_info.caps;
                return true;
            } break;
            case DBCL_ID: {
                g_network_info.caps.turns_per_second = 4;
                *out_network_code = g_network_info.id;
                *out_network_name = g_network_info.name;
                *out_network_description = g_network_info.description;
                *out_caps = &g_network_info.caps;
                return true;
            } break;
        }
    }
    return false;
}

BOOL WINAPI SnpBind(DWORD index, snp::NetFunctions** out_funcs) {
    if (out_funcs) {
        switch (index) {
            case CLNK_ID: {
                *out_funcs = &snp::g_spi_functions;
                // g_crown_link = std::make_unique<CrownLink>();
                // g_crown_link->set_mode(CrownLinkMode::CLNK);
                return true;
            } break;
            case DBCL_ID: {
                auto& snp_config = SnpConfig::instance();
                snp_config.mode = CrownLinkMode::DBCL;
                *out_funcs = &snp::g_spi_functions;
                // g_crown_link = std::make_unique<CrownLink>();
                // g_crown_link->set_mode(CrownLinkMode::DBCL);
                return true;
            } break;
        }
    }
    return false;
}

BOOL WINAPI RegisterStatusCallback(CrowServe::StatusCallback callback, bool use_status_lobby) {
    g_crown_link->register_status_callback(callback);
    return true;
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
