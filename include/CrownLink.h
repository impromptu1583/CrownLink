#pragma once

// CrownLink Public API Header

#ifdef _WIN32
#ifdef CROWNLINK_EXPORTS
#define CROWNLINK_API __declspec(dllexport)
#else
#define CROWNLINK_API __declspec(dllimport)
#endif
#else
#define CROWNLINK_API
#endif

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include <functional>
#include <string>

using u32 = std::uint32_t;

namespace CrowServe {
enum class SocketState {
    Disconnected,
    Connecting,
    Ready
};

using StatusCallback = void(*)(SocketState, void*);
}

enum class TurnsPerSecond : u32 {
    CNLK = 8,  // For Backwards Compatibility
    CLDB = 4,  // For Backwards Compatibility
    UltraLow = 4,
    Low = 6,
    Standard = 8,
    Medium = 10,
    High = 12
};

namespace CrownLink {
constexpr u32 VERSION_MAJOR = 1;
constexpr u32 VERSION_MINOR = 2;
constexpr u32 VERSION_BUILD = 0;
}  // namespace CrownLink

extern "C" {
CROWNLINK_API BOOL WINAPI SnpQuery(
    u32 index, u32* out_network_code, char** out_network_name, char** out_network_description, Caps** out_caps
);

CROWNLINK_API BOOL WINAPI SnpBind(u32 index, snp::NetFunctions** out_funcs);

CROWNLINK_API u32 WINAPI version();

CROWNLINK_API BOOL WINAPI
register_status_callback(CrowServe::StatusCallback callback, void* user_data);

CROWNLINK_API BOOL WINAPI set_turns_per_second(TurnsPerSecond turns_per_second);

CROWNLINK_API TurnsPerSecond WINAPI get_turns_per_second();

CROWNLINK_API BOOL WINAPI set_password(const char* password);

CROWNLINK_API void WINAPI get_password(char* output, u32 output_size);

CROWNLINK_API CrowServe::SocketState WINAPI get_status();

CROWNLINK_API CrowServe::SocketState WINAPI set_status_lobby(bool enable);

CROWNLINK_API CrowServe::SocketState WINAPI set_map_name_edit(bool enable);
}