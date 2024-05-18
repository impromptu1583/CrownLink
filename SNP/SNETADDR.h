#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SNETADDR_H
#include <string>

struct SNETADDR {
    SNETADDR() = default;
    SNETADDR(BYTE* addr)
    {
        memcpy(&address, addr, sizeof(SNETADDR));
    }
    SNETADDR(std::string ID) {
        memset(&address, 0, sizeof(SNETADDR));
        memcpy(&address, ID.c_str(), sizeof(SNETADDR));
    };

    BYTE address[16];

    int compare(SNETADDR other) {
        return strncmp((const char*)address, (const char*)other.address, 16);
    }
};

struct GamePacket
{
    GamePacket() = default;
    GamePacket(SNETADDR& sender_ID, const char* recv_data, const size_t size) {
        sender = SNETADDR(sender_ID);
        timeStamp = GetTickCount();
        packetSize = size;
        memcpy(data, recv_data, size);
    };
    SNETADDR sender;
    int packetSize;
    DWORD timeStamp;
    char data[512];
};

