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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Header, peer_id)

struct Ping {
    Header header;
    std::string timestamp;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Ping, header, timestamp)

struct Pong {
    Header header;
    std::string timestamp;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Pong, header, timestamp)

struct JuiceLocalDescription {
    Header header;
    std::string sdp;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceLocalDescription, header, sdp)

struct JuiceCandidate {
    Header header;
    std::string candidate;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceCandidate, header, candidate)

struct JuiceDone {
    Header header;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceDone, header)

class P2PProtocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<char> message, const Handler& handler) {
        
    }
};

}