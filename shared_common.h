#pragma once


#include <string>
#include <concepts>
#include <vector>
#include <unordered_map>
#include <iostream>
#include <format>
#include <initializer_list>
#include <fstream>
#include <chrono>
#include <mutex>
#include <shared_mutex>
#include <optional>

#include <filesystem>
namespace fs = std::filesystem;

using namespace std::literals;

#define EnumStringCase(X) case X: return #X

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


constexpr const u8    MAJOR_VERSION = 1;
constexpr const u8    MINOR_VERSION = 0;
constexpr const u8    BUILD_VERSION = 0;
inline std::string CL_VERSION_STRING() {
    return std::format("{}.{}.{}", MAJOR_VERSION, MINOR_VERSION, BUILD_VERSION);
}
inline u32 CL_VERSION_NUMBER() {
    return MAJOR_VERSION * 256 * 256 + MINOR_VERSION * 256 + BUILD_VERSION;
}

inline u32 get_tick_count() {
    using namespace std::literals;
    return static_cast<u32>(std::chrono::system_clock::now().time_since_epoch() / 1s);
}

