#pragma once
#include <chrono>
#include <storm/types.hpp>

#include "crownlink/version.h"  // generated from the latest git tag

#include "include/ConnectionState.h"
using namespace std::literals;

#define EnumStringCase(X) \
    case X:               \
        return #X

inline constexpr u32 CL_VERSION_NUMBER =
    (static_cast<u32>(CL_VERSION_MAJOR) << 16) | (static_cast<u32>(CL_VERSION_MINOR) << 8) |
    static_cast<u32>(CL_VERSION_PATCH);

inline u32 get_tick_count() {
    using namespace std::literals;
    return static_cast<u32>(std::chrono::system_clock::now().time_since_epoch() / 1s);
}

inline constexpr u32 MaxPacketSize = 512;