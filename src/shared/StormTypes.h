#pragma once

#include "simdutf.h"

#include <storm/util/snet.hpp>

#include <rfl.hpp>

#include "types.h"
#include "crownlink/TurnsPerSecond.h"

#include <algorithm>
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

inline std::string uuid_string(const NetAddress& address) {
    return std::format(
        "{:02x}{:02x}{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}-{:02x}{:02x}{:02x}{:02x}{:02x}{:02x}",
        address.bytes[0], address.bytes[1], address.bytes[2], address.bytes[3], address.bytes[4], address.bytes[5],
        address.bytes[6], address.bytes[7], address.bytes[8], address.bytes[9], address.bytes[10], address.bytes[11],
        address.bytes[12], address.bytes[13], address.bytes[14], address.bytes[15]
    );
}

inline std::ostream& operator<<(std::ostream& out, const NetAddress& address) {
    return out << uuid_string(address);
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

inline bool operator==(const AdFile& a1, const AdFile& a2) {
    return (
        a1.game_info == a2.game_info && memcmp(a1.extra_bytes, a2.extra_bytes, sizeof(a1.extra_bytes)) == 0 &&
        a1.turns_per_second == a2.turns_per_second
    );
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

namespace rfl {

// NetAddress travels as a 16-byte CBOR byte string; short strings are copied partially (lenient, matches the previous from_json)
template <>
struct Reflector<::NetAddress> {
    using ReflType = Bytestring;
    static ::NetAddress to(const ReflType& value) noexcept {
        NetAddress address{};
        const auto copy_size = std::min(value.size(), address.bytes.size());
        std::memcpy(address.bytes.data(), value.data(), copy_size);
        return address;
    }
    static ReflType from(const ::NetAddress& value) noexcept {
        return Bytestring(
            reinterpret_cast<const std::byte*>(value.bytes.data()),
            reinterpret_cast<const std::byte*>(value.bytes.data()) + value.bytes.size()
        );
    }
};

}  // namespace rfl

struct GameInfoWire {
    u32 game_index{};
    u32 game_state{};
    u32 creation_time{};
    NetAddress host{};
    u32 host_latency{};
    u32 host_last_time{};
    u32 category_bits{};
    std::string game_name{};
    std::string game_description{};
    u32 extra_bytes{};
    u32 program_id{};
    u32 version_id{};

    static GameInfoWire from_class(const GameInfo& game) noexcept {
        GameInfoWire wire{};
        wire.game_index = game.game_index;
        wire.game_state = game.game_state;
        wire.creation_time = game.creation_time;
        wire.host = game.host;
        wire.host_latency = game.host_latency;
        wire.host_last_time = game.host_last_time;
        wire.category_bits = game.category_bits;
        char name_buffer[256]{};
        simdutf::convert_latin1_to_utf8(std::string_view(game.game_name), std::span(name_buffer));
        wire.game_name = name_buffer;
        char description_buffer[256]{};
        simdutf::convert_latin1_to_utf8(std::string_view(game.game_description), std::span(description_buffer));
        wire.game_description = description_buffer;
        wire.extra_bytes = game.extra_bytes;
        wire.program_id = game.program_id;
        wire.version_id = game.version_id;
        return wire;
    }

    GameInfo to_class() const noexcept {
        GameInfo game{};
        game.game_index = game_index;
        game.game_state = game_state;
        game.creation_time = creation_time;
        game.host = host;
        game.host_latency = host_latency;
        game.host_last_time = host_last_time;
        game.category_bits = category_bits;
        simdutf::convert_utf8_to_latin1(game_name, std::span(game.game_name));
        simdutf::convert_utf8_to_latin1(game_description, std::span(game.game_description));
        game.extra_bytes = extra_bytes;
        game.program_id = program_id;
        game.version_id = version_id;
        return game;
    }
};

struct AdFileWire {
    GameInfo game_info{};
    rfl::Bytestring extra_bytes{};
    u32 crownlink_mode{};

    static AdFileWire from_class(const AdFile& ad_file) noexcept {
        AdFileWire wire{};
        wire.game_info = ad_file.game_info;
        wire.extra_bytes = rfl::Bytestring(
            reinterpret_cast<const std::byte*>(ad_file.extra_bytes),
            reinterpret_cast<const std::byte*>(ad_file.extra_bytes) + sizeof(ad_file.extra_bytes)
        );
        wire.crownlink_mode = ad_file.turns_per_second;
        return wire;
    }

    AdFile to_class() const noexcept {
        AdFile ad_file{};
        ad_file.game_info = game_info;
        const auto copy_size = std::min(extra_bytes.size(), sizeof(ad_file.extra_bytes));
        std::memcpy(ad_file.extra_bytes, extra_bytes.data(), copy_size);
        ad_file.turns_per_second = crownlink_mode;
        return ad_file;
    }
};

namespace rfl::parsing {
template <class R, class W, class P>
struct Parser<R, W, ::GameInfo, P> : public CustomParser<R, W, P, ::GameInfo, GameInfoWire> {};

template <class R, class W, class P>
struct Parser<R, W, ::AdFile, P> : public CustomParser<R, W, P, ::AdFile, AdFileWire> {};
}  // namespace rfl::parsing