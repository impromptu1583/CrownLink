#pragma once

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <filesystem>

inline const std::filesystem::path g_starcraft_dir = [] {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(0, buffer, MAX_PATH);
    return std::filesystem::path{buffer}.parent_path();
}();

struct NetAddress;
namespace P2P {
enum class MessageType;
}
namespace CrownLinkProtocol {
enum class MessageType;
}

#include "spdlog/async.h"
#include "spdlog/fmt/bin_to_hex.h"
#include "spdlog/fmt/ostr.h"
#include "spdlog/sinks/daily_file_sink.h"
#include "spdlog/sinks/rotating_file_sink.h"
#include "spdlog/spdlog.h"

template <>
struct fmt::formatter<NetAddress> : ostream_formatter {};

template <>
struct fmt::formatter<P2P::MessageType> : ostream_formatter {};

template <>
struct fmt::formatter<CrownLinkProtocol::MessageType> : ostream_formatter {};