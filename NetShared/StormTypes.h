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
    DWORD dwSize;                 // Size of this structure  // sizeof(CAPS)
    DWORD dwUnk_0x04;             // Some flags?
    DWORD maxmessagesize;         // Size of the packet buffer, must be between 128 and 512
    DWORD dwUnk_0x0C;             // Unknown
    DWORD dwDisplayedPlayerCount; // Displayed player count in the mode selection list
    DWORD dwUnk_0x14;             // some kind of timeout or timer related
    DWORD dwPlayerLatency;        // ... latency?
    DWORD dwPlayerCount;          // the number of players that can participate, must be between 1 and 20
    DWORD dwCallDelay;            // the number of calls before data is sent over the network // between 2 and 8; single player is set to 1
};

struct client_info {
    DWORD dwSize; // 60
    char* pszName;
    char* pszVersion;
    DWORD dwProduct;
    DWORD dwVerbyte;
    DWORD dwUnk5;
    DWORD dwMaxPlayers;
    DWORD dwUnk7;
    DWORD dwUnk8;
    DWORD dwUnk9;
    DWORD dwUnk10; // 0xFF
    char* pszCdKey;
    char* pszCdOwner;
    DWORD dwIsShareware;
    DWORD dwLangId;
};

struct user_info {
    DWORD dwSize; // 16
    char* pszPlayerName;
    char* pszUnknown;
    DWORD dwUnknown;
};

struct battle_info {
    DWORD dwSize;   // 92
    DWORD dwUnkType;
    HWND  hFrameWnd;
    void* pfnBattleGetResource;
    void* pfnBattleGetErrorString;
    void* pfnBattleMakeCreateGameDialog;
    void* pfnBattleUpdateIcons;
    DWORD dwUnk_07;
    void* pfnBattleErrorDialog;
    void* pfnBattlePlaySound;
    DWORD dwUnk_10;
    void* pfnBattleGetCursorLink;
    DWORD dwUnk_12;
    void* pfnUnk_13;
    DWORD dwUnk_14;
    void* pfnBattleMakeProfileDialog;
    char* pszProfileStrings;
    void* pfnBattleDrawProfileInfo;
    void* pfnUnk_18;
    DWORD dwUnk_19;
    void* pfnUnk_20;
    void* pfnUnk_21;
    void* pfnBattleSetLeagueName;
};

struct module_info {
    DWORD dwSize; // 20
    char* pszVersionString;
    char* pszModuleName;
    char* pszMainArchive;
    char* pszPatchArchive;
};

struct game {
    DWORD     dwIndex;
    DWORD     dwGameState;
    DWORD     dwUnk_08; // creation time
    NetAddress  saHost;
    DWORD     dwUnk_1C; // host latency
    DWORD     dwTimer;
    DWORD     dwUnk_24; // game category bits
    char      szGameName[128];
    char      szGameStatString[128];
    game* pNext;
    void* pExtra;
    DWORD     dwExtraBytes;
    DWORD     dwProduct;
    DWORD     dwVersion;
};

struct storm_head {
    WORD wChecksum;
    WORD wLength;
    WORD wSent;
    WORD wReceived;
    BYTE bCommandClass;
    BYTE bCommandType;
    BYTE bPlayerId;
    BYTE bFlags;
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