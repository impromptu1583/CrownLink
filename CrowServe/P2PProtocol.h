#pragma once

#include "../NetShared/StormTypes.h"

namespace P2P {

enum class MessageType {
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

class P2PProtocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<char> message, const Handler& handler) {
        
    }
};

}