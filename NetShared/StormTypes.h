#pragma once
#include "../shared_common.h"
#include <nlohmann/json.hpp>
using Json = nlohmann::json;

#include <numeric>

struct NetAddress {
    u8 bytes[16]{};

    bool operator==(const NetAddress&) const = default;
    bool operator<(const NetAddress& other) const {
        if (memcmp(bytes, other.bytes, sizeof(NetAddress)) < 0) {
            return true;
        }
        return false;
    };

    bool is_zero() const {
        return std::accumulate(std::begin(bytes), std::end(bytes), 0) == 0;
    }

    std::string uuid_string() const {
        char str[37] = {};
        sprintf(str, 
        "%02x%02x%02x%02x-%02x%02x-%02x%02x-%02x%02x-%02x%02x%02x%02x%02x%02x", 
            bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7],
            bytes[8], bytes[9], bytes[10], bytes[11], bytes[12], bytes[13], bytes[14], bytes[15]
        );
        return str;
    }

    friend std::ostream& operator<< (std::ostream& out, const NetAddress& address) {
        return out << address.uuid_string();
    }
};

inline void to_json(Json& j, const NetAddress& address) {
    j = Json{{"Id",Json::binary_t(std::vector<u8>{address.bytes,address.bytes + sizeof(NetAddress)})}};
}

inline void from_json(const Json& j, NetAddress& address) {
    // TODO error handling
    auto id = j["Id"];
    memcpy(address.bytes,id.get_binary().data(),sizeof(address.bytes));
}

template <>
struct std::hash<NetAddress> {
    size_t operator()(const NetAddress& address) const {
        size_t hash = 0;
        for (char c : address.bytes) {
            hash <<= 1;
            hash ^= std::hash<char>{}(c);
        }
        return hash;
    }
};

struct CAPS {
    u32 size;
    u32 flags;             // 0x20000003. 0x00000001 = page locked buffers, 0x00000002 = basic interface, 0x20000000 = release mode
    u32 max_message_size;  // must be between 128 and 512
    u32 max_queue_size;
    u32 max_players;
    u32 bytes_per_second;
    u32 latency;
    u32 turns_per_second; // the turn_per_second and turns_in_transit values
    u32 turns_in_transit; // in the appended mpq file seem to be used instead
};

struct client_info {
    u32   size; // 60
    char* program_name;
    char* program_description;
    u32   program_id;
    u32   version_id;
    u32   dwUnk5;
    u32   max_players;
    u32   dwUnk7;
    u32   dwUnk8;
    u32   dwUnk9;
    u32   dwUnk10; // 0xFF
    char* cd_key;
    char* cd_owner;
    u32   is_shareware;
    u32   language_id;
};

struct user_info {
    u32   size; // 16
    char* player_name;
    char* player_description;
    u32   dwUnknown;
};

struct battle_info {
    u32   size;   // 92
    u32   dwUnkType;
    void* hFrameWnd;
    void* pfnBattleGetResource;
    void* pfnBattleGetErrorString;
    void* pfnBattleMakeCreateGameDialog;
    void* pfnBattleUpdateIcons;
    u32   dwUnk_07;
    void* pfnBattleErrorDialog;
    void* pfnBattlePlaySound;
    u32   dwUnk_10;
    void* pfnBattleGetCursorLink;
    u32   dwUnk_12;
    void* pfnUnk_13;
    u32   dwUnk_14;
    void* pfnBattleMakeProfileDialog;
    char* pszProfileStrings;
    void* pfnBattleDrawProfileInfo;
    void* pfnUnk_18;
    u32   dwUnk_19;
    void* pfnUnk_20;
    void* pfnUnk_21;
    void* pfnBattleSetLeagueName;
};

struct module_info {
    u32   size; // 20
    char* version_string;
    char* executable_file;
    char* original_archive_file;
    char* patch_archive_file;
};

struct game {
    u32     game_index;
    u32     game_state;
    u32     creation_time;
    NetAddress  host;
    u32     host_latency;
    u32     host_last_time;
    u32     category_bits;
    char    game_name[128];
    char    game_description[128];
    game*   pNext;
    void*   pExtra;
    u32     extra_bytes;
    u32     program_id;
    u32     version_id;
};

inline bool operator== (const game& g1, const game& g2) {
    return (
        g1.game_index == g2.game_index &&
        g1.game_state == g2.game_state &&
        g1.creation_time == g2.creation_time &&
        g1.host == g2.host &&
        g1.host_latency == g2.host_latency &&
        g1.host_last_time == g2.host_last_time &&
        g1.category_bits == g2.category_bits &&
        strncmp(g1.game_name, g2.game_name, sizeof(g1.game_name)) == 0 &&
        strncmp(g1.game_description, g2.game_description, sizeof(g1.game_description)) == 0 &&
        g1.extra_bytes == g2.extra_bytes &&
        g1.program_id == g2.program_id &&
        g1.version_id == g2.version_id
    );
}

inline void to_json(Json& j, const game& g) {
    j = Json{
        {"game_index", g.game_index},
        {"game_state", g.game_state},
        {"creation_time", g.creation_time},
        {"host", g.host},
        {"host_latency", g.host_latency},
        {"host_last_time", g.host_last_time},
        {"category_bits", g.category_bits},
        {"game_name", g.game_name},
        {"game_description", g.game_description},
        {"extra_bytes", g.extra_bytes},
        {"program_id", g.program_id},
        {"version_id", g.version_id}
    };
}

inline void from_json (const Json& j, game& g) {
    j.at("game_index").get_to(g.game_index);
    j.at("game_state").get_to(g.game_state);
    j.at("creation_time").get_to(g.creation_time);
    j.at("host").get_to(g.host);
    j.at("host_latency").get_to(g.host_latency);
    j.at("host_last_time").get_to(g.host_last_time);
    j.at("category_bits").get_to(g.category_bits);

    auto name = j["game_name"].get<std::string>();
    memcpy(g.game_name, name.c_str(), sizeof(g.game_name));
    auto description = j["game_description"].get<std::string>();
    memcpy(g.game_description, description.c_str(), sizeof(g.game_description));

    j.at("extra_bytes").get_to(g.extra_bytes);
    j.at("program_id").get_to(g.program_id);
    j.at("version_id").get_to(g.version_id);
    g.pNext = nullptr;
    g.pExtra = nullptr;
}

enum class CrownLinkMode {
    CLNK, // standard version
    DBCL  // double brain cells version
};

inline std::string to_string(CrownLinkMode value) {
    switch (value) {
        EnumStringCase(CrownLinkMode::CLNK);
        EnumStringCase(CrownLinkMode::DBCL);
    }
    return std::to_string((s32) value);
}

struct AdFile {
    game game_info{};
    char extra_bytes[32]{};
    CrownLinkMode crownlink_mode{};
    bool          mark_for_removal = false;
    std::string   original_name{""};
    bool          is_same_owner(const AdFile& other) const { return game_info.host == other.game_info.host;}
};

inline bool operator==(const AdFile& a1, const AdFile& a2) {
    return (
        a1.game_info == a2.game_info &&
        memcmp(a1.extra_bytes, a2.extra_bytes, sizeof(a1.extra_bytes)) == 0 &&
        a1.crownlink_mode == a2.crownlink_mode
    );
}

inline void to_json(Json& j, const AdFile& ad_file) {
    j = Json{
        {"game_info", ad_file.game_info},
        {"extra_bytes", Json::binary({ad_file.extra_bytes,ad_file.extra_bytes+32})},
        {"crownlink_mode", ad_file.crownlink_mode}
    };
}

inline void from_json(const Json& j, AdFile& ad_file) {
    j.at("game_info").get_to(ad_file.game_info);
    j.at("crownlink_mode").get_to(ad_file.crownlink_mode);
    auto a = j["extra_bytes"].get_binary();
    std::copy(a.begin(), a.end(), ad_file.extra_bytes);

    // auto extra_bytes = j["extra_bytes"].get<std::string>();
    // memcpy(ad_file.extra_bytes, extra_bytes.c_str(), sizeof(ad_file.extra_bytes));
}

enum class GamePacketType : u8 {
    System,
    Message,
    Turn,
    Types,
};

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
    }
    return std::to_string((u8) value);
}

enum class GamePacketSubType : u8 {
    Unused,
    InitialContact,
    CircuitCheck,
    CircuiteCheckResponse,
    Ping,
    PingResponse,
    PlayerInfo,
    PlayerJoin,
    PlayerJoinAcceptStart,
    PlayerJoinAcceptDone,
    PlayerJoinReject,
    PlayerLeave,
    DropPlayer,
    NewGameOwner,
    Messages
};

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
    return std::to_string((u8) value);
}

enum class GamePacketFlags : u8 {
    Acknowledgement = 0x01,
    ResendRequest = 0x02,
    Forwareded = 0x04
};

inline std::string to_string(GamePacketFlags value) {
    std::string out;
    if ((u8)value & (u8)GamePacketFlags::Acknowledgement) {out += "Ack";}
    if ((u8)value & (u8)GamePacketFlags::ResendRequest) {out += "Resend";}
    if ((u8)value & (u8)GamePacketFlags::Forwareded) {out += "Fwd";}
    return out;
}

struct GamePacketHeader {
    u16 checksum;
    u16 size;
    u16 sequence;
    u16 ack_sequence;
    GamePacketType type;
    GamePacketSubType sub_type;
    u8 player_id;
    GamePacketFlags flags;
};

inline std::string to_string(GamePacketHeader& header) {
    return std::format(
        "sz:{} seq:{} ack:{} tp:{} st:{} pid:{} flags:{}", header.size,
        header.sequence, header.ack_sequence, to_string(header.type), to_string(header.sub_type), header.player_id,
        to_string(header.flags)
    );
}

struct SystemPlayerJoin_PlayerInfo {
    u32  bytes;
    u32  player_id;
    bool gameowner;
    u32  flags;
    u32  starting_turn;
    NetAddress address;
    char       name_description[256];
};

struct GamePacketData {
    GamePacketHeader header;
    char payload[500]{};
};

struct GamePacket {
    NetAddress sender{};
    u32 size = 0;
    u32 timestamp = 0;
    GamePacketData data;

    GamePacket() = default;
    GamePacket(const NetAddress& sender_id, const char* recv_data, const size_t size)
        : sender{sender_id}, timestamp{get_tick_count()}, size{(u32)size} {
        memcpy(&data, recv_data, size < 500 ? size : 500);
    };
};