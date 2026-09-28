#pragma once

#include "shared/StormTypes.h"
#include "shared/common.h"

#include <rfl.hpp>

enum class JuiceAgentType {
    RelayFallback,  // Legacy behavior
    P2P,
    Relay,
};

namespace rfl {

// As unterlying integer (go fxamacker decodes to go enum)
template <>
struct Reflector<::JuiceAgentType> {
    using ReflType = u32;
    static ::JuiceAgentType to(const ReflType& value) noexcept { return static_cast<::JuiceAgentType>(value); }
    static ReflType from(const ::JuiceAgentType& value) noexcept { return std::to_underlying(value); }
};

}  // namespace rfl

namespace P2P {

enum class MessageType {
    ConnectionRequest = 1,
    Pong,
    JuiceLocalDescription,
    JuiceCandidate,
    JuiceDone,
};

std::ostream& operator<<(std::ostream& out, MessageType message_type);

struct Header {
    NetAddress peer_id;
    JuiceAgentType agent_type;
};

struct ConnectionRequest {
    Header header;
    u32 counter = 0;
    inline MessageType type() const { return MessageType::ConnectionRequest; }
};

struct Pong {
    Header header;
    std::string timestamp;
    inline MessageType type() const { return MessageType::Pong; }
};

struct JuiceLocalDescription {
    Header header;
    std::string sdp;
    inline MessageType type() const { return MessageType::JuiceLocalDescription; }
};

struct JuiceCandidate {
    Header header;
    std::string candidate;
    inline MessageType type() const { return MessageType::JuiceCandidate; }
};

struct JuiceDone {
    Header header;
    inline MessageType type() const { return MessageType::JuiceDone; }
};

class Protocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<u8> message, const Handler& handler) {
        switch (MessageType(message_type)) {
            case MessageType::ConnectionRequest: {
                ConnectionRequest deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::Pong: {
                Pong deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::JuiceLocalDescription: {
                JuiceLocalDescription deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::JuiceCandidate: {
                JuiceCandidate deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::JuiceDone: {
                JuiceDone deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
        }
    }
};

}  // namespace P2P