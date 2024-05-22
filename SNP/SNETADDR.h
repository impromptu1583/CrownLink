#pragma once
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define SNETADDR_H
#include <string>
#include "base64.hpp"
#include "Types.h"

struct SNetAddr {
    u8 address[16]{};

    SNetAddr() = default;
    SNetAddr(u8* addr) {
        memcpy_s(&address, sizeof(address), addr, sizeof(SNetAddr));
    }

    SNetAddr(const std::string& id) {
        memcpy_s(&address, sizeof(address), id.c_str(), sizeof(SNetAddr));
    };

    int compare(const SNetAddr& other) {
        return memcmp(&address, &other.address, sizeof(SNetAddr));
    }

    std::string b64() const volatile {
        return base64::to_base64(std::string((char*)address, sizeof(SNetAddr)));
    }
};

struct GamePacket {
    SNetAddr sender{};
    u32 size = 0;
    u32 timestamp = 0;
    char data[512]{};

    GamePacket() = default;
    GamePacket(const SNetAddr& sender_id, const char* recv_data, const size_t size)
        : sender{sender_id}, timestamp{GetTickCount()}, size{size} {
        memcpy_s(data, sizeof(data), recv_data, size);
    };
};

