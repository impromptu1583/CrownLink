#pragma once
#include <chrono>
#include <concepts>
#include <filesystem>
#include <format>
#include <fstream>
#include <initializer_list>
#include <iostream>
#include <mutex>
#include <optional>
#include <shared_mutex>
#include <string>
#include <unordered_map>
#include <vector>
namespace fs = std::filesystem;

using namespace std::literals;

#define EnumStringCase(X) \
    case X:               \
        return #X

using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

using s8 = signed char;
using s16 = signed short;
using s32 = signed int;
using s64 = signed long long;

using f32 = float;
using f64 = double;

inline constexpr u8  MAJOR_VERSION = 1;
inline constexpr u8  MINOR_VERSION = 0;
inline constexpr u8  BUILD_VERSION = 7;
inline const auto    CL_VERSION_STRING = std::format("{}.{}.{}", MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION);
inline constexpr u32 CL_VERSION_NUMBER =
    (static_cast<u32>(MAJOR_VERSION) << 16) | (static_cast<u32>(MINOR_VERSION) << 8) | static_cast<u32>(BUILD_VERSION);

inline u32 get_tick_count() {
    using namespace std::literals;
    return static_cast<u32>(std::chrono::system_clock::now().time_since_epoch() / 1s);
}
