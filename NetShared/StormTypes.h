#pragma once
#include "Common.h"

struct NetAddress {
    u8 bytes[16]{};

    NetAddress() = default;
    NetAddress(u8* addr) {
        memcpy_s(&bytes, sizeof(bytes), addr, sizeof(NetAddress));
    }

    NetAddress(const std::string& id) {
        memcpy_s(&bytes, sizeof(bytes), id.c_str(), sizeof(NetAddress));
    };

    std::string b64() const {
        return base64::to_base64(std::string((char*)bytes, sizeof(NetAddress)));
    }
    
    bool operator==(const NetAddress&) const = default;
};

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
    u32   dwSize; // 60
    char* pszName;
    char* pszVersion;
    u32   dwProduct;
    u32   dwVerbyte;
    u32   dwUnk5;
    u32   dwMaxPlayers;
    u32   dwUnk7;
    u32   dwUnk8;
    u32   dwUnk9;
    u32   dwUnk10; // 0xFF
    char* pszCdKey;
    char* pszCdOwner;
    u32   dwIsShareware;
    u32   dwLangId;
};

struct user_info {
    u32   dwSize; // 16
    char* pszPlayerName;
    char* pszUnknown;
    u32   dwUnknown;
};

struct battle_info {
    u32   dwSize;   // 92
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
    u32   dwSize; // 20
    char* pszVersionString;
    char* pszModuleName;
    char* pszMainArchive;
    char* pszPatchArchive;
};

struct game {
    u32     dwIndex;
    u32     dwGameState;
    u32     dwUnk_08; // creation time
    NetAddress  saHost;
    u32     dwUnk_1C; // host latency
    u32     dwTimer;
    u32     dwUnk_24; // game category bits
    char    szGameName[128];
    char    szGameStatString[128];
    game*   pNext;
    void*   pExtra;
    u32     dwExtraBytes;
    u32     dwProduct;
    u32     dwVersion;
};

struct storm_head {
    u16 wChecksum;
    u16 wLength;
    u16 wSent;
    u16 wReceived;
    u8  bCommandClass;
    u8  bCommandType;
    u8  bPlayerId;
    u8  bFlags;
};


struct GamePacket {
    NetAddress sender{};
    u32 size = 0;
    u32 timestamp = 0;
    char data[512]{};

    GamePacket() = default;
    GamePacket(const NetAddress& sender_id, const char* recv_data, const size_t size)
        : sender{sender_id}, timestamp{GetTickCount()}, size{size} {
        memcpy_s(data, sizeof(data), recv_data, size);
    };
};

struct AdFile {
    game game_info{};
    char extra_bytes[32]{};
};