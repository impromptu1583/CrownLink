#pragma once
#include "CrowServe.hpp"

#include "../NetShared/StormTypes.h"

enum class P2PMessageType {
    Ping = 1,
    Pong,
    JuiceLocalDescription,
    JuiceCandidate,
    JuiceDone,
};

struct Header {
    NetAddress peer_id;    
};

struct Ping {
    Header header;
    std::string timestamp;
};

struct Pong {
    Header header;
    std::string timestamp;
};

struct JuiceLocalDescription {
    Header header;
    std::string sdp;
};

struct JuiceCandidate {
    Header header;
    std::string candidate;
};

struct JuiceDone {
    Header header;
};
