#pragma once

#include "common.h"
#include "../NetShared/StormTypes.h"

namespace P2P {

enum class MessageType {
    ConnectionRequest = 1,
    Pong,
    JuiceLocalDescription,
    JuiceCandidate,
    JuiceDone,
};

std::ostream& operator<<(std::ostream& out, const MessageType& message_type);

struct Header {
    NetAddress peer_id;    
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Header, peer_id)

struct ConnectionRequest {
    Header header;
    std::string timestamp;
    inline MessageType type() const { return MessageType::ConnectionRequest; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConnectionRequest, header, timestamp)

struct Pong {
    Header header;
    std::string timestamp;
    inline MessageType type() const { return MessageType::Pong; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(Pong, header, timestamp)

struct JuiceLocalDescription {
    Header header;
    std::string sdp;
    inline MessageType type() const { return MessageType::JuiceLocalDescription; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceLocalDescription, header, sdp)

struct JuiceCandidate {
    Header header;
    std::string candidate;
    inline MessageType type() const { return MessageType::JuiceCandidate; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceCandidate, header, candidate)

struct JuiceDone {
    Header header;
    inline MessageType type() const { return MessageType::JuiceDone; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(JuiceDone, header)

class Protocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<u8> message, const Handler& handler) {
        switch (MessageType(message_type)) {
            case MessageType::ConnectionRequest: {
                ConnectionRequest deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::Pong: {
                Pong deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::JuiceLocalDescription: {
                JuiceLocalDescription deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::JuiceCandidate: {
                JuiceCandidate deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::JuiceDone: {
                JuiceDone deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            default: {
            }
        }
        
    }
};

}