#pragma once

constexpr const char* CL_VERSION = "0.5.0";

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

inline u32 get_tick_count() {
    using namespace std::literals;
    return static_cast<u32>(std::chrono::system_clock::now().time_since_epoch() / 1s);
}

