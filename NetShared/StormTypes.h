#pragma once

#define JSON_DIAGNOSTICS 1

#include <nlohmann/json.hpp>
#include "simdutf.h"

#include <storm/util/snet.hpp>

#include "../types.h"
#include "../include/TurnsPerSecond.h"
using Json = nlohmann::json;

#include <algorithm>
#include <array>
#include <cstring>
#include <format>
#include <iostream>
#include <ranges>
#include <span>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

// storm ABI structs live in <storm/util/snet.hpp>; this header holds the CrownLink-side
// serialization glue for them

inline void to_json(Json& j, const NetAddress& address) {
    j = Json::binary_t(std::vector<u8>(address.bytes.begin(), address.bytes.end()));
}

inline void from_json(const Json& j, NetAddress& address) {
    const auto& binary_data = j.get_binary();
    const auto copy_size = std::min(binary_data.size(), address.bytes.size());
    std::copy_n(binary_data.begin(), copy_size, address.bytes.data());
}

template <>
struct std::hash<NetAddress> {
    size_t operator()(const NetAddress& address) const {
        size_t hash = 0;
        for (auto c : address.bytes) {
            hash <<= 1;
            hash ^= std::hash<u8>{}(c);
        }
        return hash;
    }
};

inline void to_json(Json& j, const GameInfo& g) {
    char game_name[256]{};
    simdutf::convert_latin1_to_utf8(std::string_view(g.game_name), std::span(game_name));
    char game_desc[256]{};
    simdutf::convert_latin1_to_utf8(std::string_view(g.game_description), std::span(game_desc));
    j = Json{
        {"game_index", g.game_index},
        {"game_state", g.game_state},
        {"creation_time", g.creation_time},
        {"host", g.host},
        {"host_latency", g.host_latency},
        {"host_last_time", g.host_last_time},
        {"category_bits", g.category_bits},
        {"game_name", game_name},
        {"game_description", game_desc},
        {"extra_bytes", g.extra_bytes},
        {"program_id", g.program_id},
        {"version_id", g.version_id}
    };
}

inline void from_json(const Json& j, GameInfo& g) {
    j.at("game_index").get_to(g.game_index);
    j.at("game_state").get_to(g.game_state);
    j.at("creation_time").get_to(g.creation_time);
    j.at("host").get_to(g.host);
    j.at("host_latency").get_to(g.host_latency);
    j.at("host_last_time").get_to(g.host_last_time);
    j.at("category_bits").get_to(g.category_bits);

    auto name = j["game_name"].get<std::string>();
    simdutf::convert_utf8_to_latin1(name, std::span(g.game_name));
    auto description = j["game_description"].get<std::string>();
    simdutf::convert_utf8_to_latin1(description, std::span(g.game_description));

    j.at("extra_bytes").get_to(g.extra_bytes);
    j.at("program_id").get_to(g.program_id);
    j.at("version_id").get_to(g.version_id);
    g.pNext = nullptr;
    g.pExtra = nullptr;
}

inline bool operator==(const AdFile& a1, const AdFile& a2) {
    return (
        a1.game_info == a2.game_info && memcmp(a1.extra_bytes, a2.extra_bytes, sizeof(a1.extra_bytes)) == 0 &&
        a1.turns_per_second == a2.turns_per_second
    );
}

inline void to_json(Json& j, const AdFile& ad_file) {
    j = Json{
        {"game_info", ad_file.game_info},
        {"extra_bytes", Json::binary({ad_file.extra_bytes, ad_file.extra_bytes + 32})},
        {"crownlink_mode", ad_file.turns_per_second}
    };
}

inline void from_json(const Json& j, AdFile& ad_file) {
    j.at("game_info").get_to(ad_file.game_info);
    j.at("crownlink_mode").get_to(ad_file.turns_per_second);
    auto extra_bytes = j["extra_bytes"].get_binary();
    std::copy(extra_bytes.begin(), extra_bytes.end(), ad_file.extra_bytes);
}

inline std::string to_string(GamePacketType value) {
    switch (value) {
        case GamePacketType::System:
            return {"Sys"};
        case GamePacketType::Message:
            return {"Msg"};
        case GamePacketType::Turn:
            return {"Turn"};
        case GamePacketType::Types:
            return {"Types"};
        case GamePacketType::CrownLink:
            return {"CrownLink"};
    }
    return std::to_string((u8)value);
}

inline std::string to_string(GamePacketSubType value) {
    switch (value) {
        case GamePacketSubType::Unused:
            return {""};
        case GamePacketSubType::InitialContact:
            return {"InitialContact"};
        case GamePacketSubType::CircuitCheck:
            return {"CircuitCheck"};
        case GamePacketSubType::CircuiteCheckResponse:
            return {"CircuiteCheckResponse"};
        case GamePacketSubType::Ping:
            return {"Ping"};
        case GamePacketSubType::PingResponse:
            return {"PingResponse"};
        case GamePacketSubType::PlayerInfo:
            return {"PlayerInfo"};
        case GamePacketSubType::PlayerJoin:
            return {"PlayerJoin"};
        case GamePacketSubType::PlayerJoinAcceptStart:
            return {"PlayerJoinAcceptStart"};
        case GamePacketSubType::PlayerJoinAcceptDone:
            return {"PlayerJoinAcceptDone"};
        case GamePacketSubType::PlayerJoinReject:
            return {"PlayerJoinReject"};
        case GamePacketSubType::PlayerLeave:
            return {"PlayerLeave"};
        case GamePacketSubType::DropPlayer:
            return {"DropPlayer"};
        case GamePacketSubType::NewGameOwner:
            return {"NewGameOwner"};
        case GamePacketSubType::Messages:
            return {"Messages"};
    }
    return std::to_string((u8)value);
}

inline std::string to_string(GamePacketFlags value) {
    std::string out;
    if ((u8)value & (u8)GamePacketFlags::Acknowledgement) {
        out += "Ack";
    }
    if ((u8)value & (u8)GamePacketFlags::ResendRequest) {
        out += "Resend";
    }
    if ((u8)value & (u8)GamePacketFlags::Forwareded) {
        out += "Fwd";
    }
    return out;
}

inline std::string to_string(GamePacketHeader& header) {
    return std::format(
        "sz:{} seq:{} ack:{} tp:{} st:{} pid:{} flags:{}", header.size, header.sequence, header.ack_sequence,
        to_string(header.type), to_string(header.sub_type), header.player_id, to_string(header.flags)
    );
}

static_assert(MaxPacketSize == sizeof(GamePacketData));

// Builds a GamePacket from raw peer data; size is clamped to what GamePacketData can hold
inline GamePacket make_game_packet(const NetAddress& sender, const char* data, size_t size) {
    GamePacket packet{};
    packet.sender = sender;
    packet.size = static_cast<u32>(std::min(size, sizeof(packet.data)));
    packet.timestamp = get_tick_count();
    memcpy(&packet.data, data, packet.size);
    return packet;
}