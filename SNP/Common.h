#pragma once
#define JSON_DIAGNOSTICS 1

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



#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#include <juice.h>
#include <base64.hpp>
#include <concurrentqueue.h>
#include <json.hpp>
using Json = nlohmann::json;

inline const fs::path g_starcraft_dir = [] {
	wchar_t buffer[MAX_PATH];
	GetModuleFileNameW(0, buffer, MAX_PATH);
	return fs::path{ buffer }.parent_path();
	}();

#include "spdlog/spdlog.h"
#include "spdlog/async.h"
#include "spdlog/sinks/daily_file_sink.h"

//#include <Storm/storm.h>

#include "../NetShared/StormTypes.h"
#include "SNPModule.h"

//#include "ThQueue.h"

constexpr const char* CL_VERSION = "0.3.8";

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
	} else if constexpr (requires { std::declval(std::stringstream) << value; }) {
		std::stringstream ss;
		ss << value;
		return ss.str();
	}
	return std::string();
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
