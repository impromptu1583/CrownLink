#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SNETADDR_H
#include <string>
#include "base64.hpp"

struct SNETADDR {
    BYTE address[16]{};

    SNETADDR() = default;
    SNETADDR(BYTE* addr) {
        memcpy_s(&address, sizeof(address), addr, sizeof(SNETADDR));
    }

    SNETADDR(const std::string& id) {
        memcpy_s(&address, sizeof(address), id.c_str(), sizeof(SNETADDR));
    };

    int compare(const SNETADDR& other) {
        return memcmp(&address, &other.address, sizeof(SNETADDR));
    }

    std::string b64() const volatile {
        return base64::to_base64(std::string((char*)address, sizeof(SNETADDR)));
    }
};

struct GamePacket {
    SNETADDR sender{};
    u32 size = 0;
    u32 timestamp = 0;
    char data[512]{};

    GamePacket() = default;
    GamePacket(const SNETADDR& sender_id, const char* recv_data, const size_t size)
        : sender{sender_id}, timestamp{GetTickCount()}, size{size} {
        memcpy_s(data, sizeof(data), recv_data, size);
    };
};

