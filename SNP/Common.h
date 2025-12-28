#pragma once

#include "../shared_common.h"

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX

#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

inline const fs::path g_starcraft_dir = [] {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(0, buffer, MAX_PATH);
    return fs::path{buffer}.parent_path();
}();