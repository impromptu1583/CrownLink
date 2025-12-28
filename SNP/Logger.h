#pragma once

#include "../NetShared/StormTypes.h"
#include "../CrowServe/CrowServe.h"

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