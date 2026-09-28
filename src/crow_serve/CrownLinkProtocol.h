#pragma once

#include "shared/StormTypes.h"
#include "shared/common.h"

#include <rfl.hpp>

#include <optional>
#include <vector>

namespace CrownLinkProtocol {

enum class MessageType {
    ConnectionRequest = 1,
    KeyExchange,
    ClientProfile,
    UpdateAvailable,
    StartAdvertising,
    StopAdvertising,
    AdvertisementsRequest,
    AdvertisementsResponse,
    EchoRequest,
    EchoResponse,
};

std::ostream& operator<<(std::ostream& out, const MessageType& message_type);

enum class Mode {
    Default,
    DoubleBrainCells,
};

struct ConnectionRequest {
    std::string preshared_key;
    bool peer_id_requested = false;
    NetAddress requested_id;
    NetAddress request_token;
    std::string lobby_password{};
    u32 product_id;
    u32 version_id;
    u32 crownlink_version;
    inline MessageType type() { return MessageType::ConnectionRequest; }
};

struct KeyExchange {
    std::string public_key;
    inline MessageType type() { return MessageType::KeyExchange; }
};

struct TurnServer {
    std::string host;
    std::string port;
    std::string username;
    std::string password;
};

// Receive only struct
struct IceCredentials {
    std::string stun_host;
    std::string stun_port;
    std::optional<std::vector<TurnServer>> turn_servers;

    const std::vector<TurnServer>& turn_server_list() const {
        static const std::vector<TurnServer> none;
        return turn_servers ? *turn_servers : none;
    }
};

struct ClientProfile {
    NetAddress peer_id;
    NetAddress token;
    IceCredentials ice_credentials;
    inline MessageType type() const { return MessageType::ClientProfile; }
};

struct UpdateAvailable {
    u32 min_clink_version;
    inline MessageType type() const { return MessageType::UpdateAvailable; }
};

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising {
    AdFile ad;
    std::string lobby_password{};
    inline MessageType type() const { return MessageType::StartAdvertising; }
};

struct StopAdvertising {
    inline MessageType type() const { return MessageType::StopAdvertising; }
};

struct AdvertisementsRequest {
    std::string lobby_password{};
    inline MessageType type() const { return MessageType::AdvertisementsRequest; }
};

struct AdvertisementsResponse {
    std::vector<AdFile> ad_files;
    inline MessageType type() const { return MessageType::AdvertisementsResponse; }
};

struct EchoRequest {
    std::string message;
    inline MessageType type() const { return MessageType::EchoRequest; }
};

struct EchoResponse {
    std::string message;
    inline MessageType type() const { return MessageType::EchoResponse; }
};

class Protocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, std::span<u8> message, const Handler& handler) {
        switch (MessageType(message_type)) {
            case MessageType::ConnectionRequest: {
                ConnectionRequest deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::KeyExchange: {
                KeyExchange deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::ClientProfile: {
                ClientProfile deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::UpdateAvailable: {
                UpdateAvailable deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::StartAdvertising: {
                StartAdvertising deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::StopAdvertising: {
                StopAdvertising deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::AdvertisementsRequest: {
                AdvertisementsRequest deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::AdvertisementsResponse: {
                AdvertisementsResponse deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::EchoRequest: {
                EchoRequest deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
            case MessageType::EchoResponse: {
                EchoResponse deserialized{};
                if (deserialize_cbor_into(deserialized, message)) {
                    handler(deserialized);
                }
            } break;
        }
    }
};

}  // namespace CrownLinkProtocol