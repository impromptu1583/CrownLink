#pragma once

#include "../shared_common.h"

#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <unistd.h>
#include <iostream>

enum class ProtocolType : u16 {
    ProtocolCrownLink = 1,
    ProtocolP2P = 2,
};

struct CrowServeHeader {
    u8 magic;
    u16 version;
    u16 protocol;
    u32 message_count;
};

struct MessageHeader {
    u64 message_size;
    u32 message_type;
};

bool temp_test();


class CrowServeSocket {
public:
    bool try_init();
    void send();
    void receive();

private:
    int m_socket = 0;

};
