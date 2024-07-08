#pragma once

#include "../shared_common.h"

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