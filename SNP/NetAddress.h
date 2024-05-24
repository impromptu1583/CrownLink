#pragma once
#include "Common.h"

struct NetAddress {
    u8 address[16]{};

    NetAddress() = default;
    NetAddress(u8* addr) {
        memcpy_s(&address, sizeof(address), addr, sizeof(NetAddress));
    }

    NetAddress(const std::string& id) {
        memcpy_s(&address, sizeof(address), id.c_str(), sizeof(NetAddress));
    };

    int compare(const NetAddress& other) {
        return memcmp(&address, &other.address, sizeof(NetAddress));
    }

    std::string b64() const volatile {
        return base64::to_base64(std::string((char*)address, sizeof(NetAddress)));
    }
    
    bool operator==(const NetAddress&) const = default;
};

template <>
struct std::hash<NetAddress> {
    size_t operator()(const NetAddress& address) const {
        size_t hash = 0;
        for (char c : address.address) {
            hash <<= 1;
            hash ^= std::hash<char>{}(c);
        }
        return hash;
    }
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

