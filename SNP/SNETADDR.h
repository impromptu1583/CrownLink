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
    GamePacket(const SNETADDR& sender_ID, size_t size, const char* dataarray) {
        sender = sender_ID;
        packetSize = static_cast<int>(size); // technically unsafe but size can never be bigger than max MTU of the connection (1500 typically)
        memcpy(data, dataarray, size);
        timeStamp = GetTickCount();
    };
    SNETADDR sender;
    int packetSize;
    DWORD timeStamp;
    char data[512];
};

