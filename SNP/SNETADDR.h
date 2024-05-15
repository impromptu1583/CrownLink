#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SNETADDR_H
#include <string>

struct SNETADDR {
    SNETADDR() {
        memset(&address, 0, sizeof(SNETADDR));
    };
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