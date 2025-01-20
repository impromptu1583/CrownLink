#pragma once

#include "../shared_common.h"

#define JSON_DIAGNOSTICS 1

#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <concurrentqueue.h>
#include <juice.h>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <nlohmann/json.hpp>

using Json = nlohmann::json;

inline const fs::path g_starcraft_dir = [] {
    wchar_t buffer[MAX_PATH];
    GetModuleFileNameW(0, buffer, MAX_PATH);
    return fs::path{buffer}.parent_path();
}();

#include "../CrowServe/CrowServe.h"
#include "../NetShared/StormTypes.h"
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

#include "SNPModule.h"
#include "config.h"

inline std::string to_string(juice_state value) {
    switch (value) {
        EnumStringCase(JUICE_STATE_DISCONNECTED);
        EnumStringCase(JUICE_STATE_GATHERING);
        EnumStringCase(JUICE_STATE_CONNECTING);
        EnumStringCase(JUICE_STATE_CONNECTED);
        EnumStringCase(JUICE_STATE_COMPLETED);
        EnumStringCase(JUICE_STATE_FAILED);
    }
    return std::to_string((s32)value);
}

inline std::string as_string(const auto& value) {
    using std::to_string;
    if constexpr (std::convertible_to<decltype(value), std::string>) {
        return value;
    } else if constexpr (requires { to_string(value); }) {
        return to_string(value);
    } /* else if constexpr (requires { std::declval(std::stringstream) << value; }) {
        std::stringstream ss;
        ss << value;
        return ss.str();
    }
    */
    return std::string();
}

enum class ColorByte : u8 {
    Revert = 1,
    Blue,
    Green,
    LightGreen,
    Gray,
    White,
    Red,
    Black
};

inline std::ostream& operator<<(std::ostream& out, ColorByte value) {
    return out << static_cast<u8>(value);
}

// NOTE: this code doesn't yet compile, but would be VERY NICE :) -Veeq7
/*
template <typename T>
concept WithToString = requires(const T& t) {
        to_string(t);
};

template <typename T>
concept WithBeginAndEnd = requires(const T& t) {
        t.begin();
        t.end();
};

template <typename T>
constexpr bool IsPair = false;

template <typename T, typename Y>
constexpr bool IsPair<std::pair<T, Y>> = true;

template <typename T>
constexpr bool IsCStyleArray = false;

template <typename T, size_t N>
constexpr bool IsCStyleArray<T[N]> = true;

template <typename T>
concept CStyleArray = IsCStyleArray<T>;

template <typename T>
concept Pair = IsPair<T>;

template <typename T>
concept PairIterable = requires(const T& t) {
        {*t.begin()} -> Pair;
        {*t.end()} -> Pair;
};

template <typename T>
concept SingleIterable = (WithBeginAndEnd<T>) && !PairIterable<T>;

inline std::ostream& operator<<(std::ostream& out, const WithToString auto& value) {
        return out << to_string(value);
}

std::ostream& operator<<(std::ostream& out, const SingleIterable auto& vec) {
        out << "[";
        bool first = true;
        for (const auto& value : vec) {
                if (first) {
                        first = false;
                } else {
                        out << ", ";
                }
                out << value;
        }
        out << "]";
        return out;
}

std::ostream& operator<<(std::ostream& out, const PairIterable auto& map) {
        out << "{";
        bool first = true;
        for (const auto& [key, value] : map) {
                if (first) {
                        first = false;
                } else {
                        out << ", ";
                }
                out << key << ": " << value;
        }
        out << "}";
        return out;
}
*/
