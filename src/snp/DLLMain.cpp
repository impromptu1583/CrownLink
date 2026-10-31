#include <juice.h>

#include "AdvertisementManager.h"
#include "Config.h"
#include "CrowServeManager.h"
#include "Globals.h"
#include "Logger.h"
#include "SNPModule.h"

BOOL WINAPI SnpQuery(
    u32 index, u32* out_network_code, char** out_network_name, char** out_network_description, Caps** out_caps
) {
    ensure_logging();
    constexpr auto provider_count = std::size(snp::provider_ids);
    // storm queries increasing indices until this returns false, so this is the normal end of enumeration
    if (index == provider_count) {
        return false;
    }
    if (index > provider_count || !out_network_code || !out_network_name || !out_network_description || !out_caps) {
        spdlog::critical(
            "SnpQuery failed, index: {} (provider count: {}), out_network_code: {}, out_network_name: {}, "
            "out_network_description: {}, out_caps: {}",
            index, provider_count, fmt::ptr(out_network_code), fmt::ptr(out_network_name),
            fmt::ptr(out_network_description), fmt::ptr(out_caps)
        );
        return false;
    }
    *out_network_code = g_network_info.id;
    *out_network_name = g_network_info.name;
    *out_network_description = g_network_info.description;
    *out_caps = &g_network_info.caps;
    return true;
}

BOOL WINAPI SnpBind(u32 index, NetFunctions** out_funcs) {
    ensure_logging();
    if (index >= std::size(snp::provider_ids) || !out_funcs) {
        spdlog::critical(
            "SnpBind failed, index: {} (provider count: {}), out_funcs: {}", index, std::size(snp::provider_ids),
            fmt::ptr(out_funcs)
        );
        return false;
    }
    snp::record_bound_provider(index);
    *out_funcs = &snp::g_spi_functions;
    return true;
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

BOOL WINAPI DllMain(HINSTANCE instance, u32 reason, LPVOID reserved) {
    switch (reason) {
        case DLL_PROCESS_ATTACH: {
            juice_set_log_handler(juice_logger);
            juice_set_log_level(JUICE_LOG_LEVEL_INFO);
        } break;
        case DLL_PROCESS_DETACH: {
            // Never destroy the context here: on process exit its threads are already dead, and on FreeLibrary
            // joining them under the loader lock deadlocks. Releasing it now keeps the CRT's static teardown away
            // from it. reserved is null for FreeLibrary, where storm always calls spi_destroy first
            if (!reserved) {
                if (g_context) {
                    spdlog::warn("Unloaded with a live context, leaking it");
                }
                release_logging();
            }
            (void)g_context.release();
        } break;
    }
    return true;
}
