#pragma once

#include "../NetShared/StormTypes.h"
#include "common.h"

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

struct Header {};

struct ConnectionRequest : Header {
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
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(
    ConnectionRequest, preshared_key, peer_id_requested, requested_id, request_token, lobby_password, product_id,
    version_id, crownlink_version
)

struct KeyExchange : Header {
    std::string public_key;
    inline MessageType type() { return MessageType::KeyExchange; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(KeyExchange, public_key)

struct TurnServer {
    std::string host;
    std::string port;
    std::string username;
    std::string password;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TurnServer, host, port, username, password)

struct IceCredentials {
    std::string stun_host;
    std::string stun_port;
    u32 turn_servers_count;
    TurnServer turn_servers[2];
};

inline void to_json(Json& j, const IceCredentials& ice_credentials) {
    j = Json{{"stun_host", ice_credentials.stun_host}, {"stun_port", ice_credentials.stun_port}};
}

inline void from_json(const Json& j, IceCredentials& ice_credentials) {
    j.at("stun_host").get_to(ice_credentials.stun_host);
    j.at("stun_port").get_to(ice_credentials.stun_port);
    if (j["turn_servers"].is_array()) {
        auto servers = j.at("turn_servers");
        for (u32 i = 0; i < 2 && i < servers.size(); i++) {
            ice_credentials.turn_servers[i] = servers[i].template get<TurnServer>();
        }
        ice_credentials.turn_servers_count = servers.size();
        if (ice_credentials.turn_servers_count > 2) {
            ice_credentials.turn_servers_count = 2;
        }
    }
}

struct ClientProfile : Header {
    NetAddress peer_id;
    NetAddress token;
    IceCredentials ice_credentials;
    inline MessageType type() const { return MessageType::ClientProfile; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClientProfile, peer_id, token, ice_credentials)

struct UpdateAvailable : Header {
    u32 min_clink_version;
    inline MessageType type() const { return MessageType::UpdateAvailable; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UpdateAvailable, min_clink_version)

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising : Header {
    AdFile ad;
    std::string lobby_password{};
    inline MessageType type() const { return MessageType::StartAdvertising; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(StartAdvertising, ad, lobby_password)

struct StopAdvertising : Header {
    inline MessageType type() const { return MessageType::StopAdvertising; }
};

inline void to_json(Json& j, const StopAdvertising& message) {
    // do nothing because this is an empty type
}

inline void from_json(const Json& j, StopAdvertising& message) {
    // do nothing because this is an empty type
}

struct AdvertisementsRequest : Header {
    std::string lobby_password{};
    inline MessageType type() const { return MessageType::AdvertisementsRequest; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AdvertisementsRequest, lobby_password)

struct AdvertisementsResponse : Header {
    std::vector<AdFile> ad_files;
    inline MessageType type() const { return MessageType::AdvertisementsResponse; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(AdvertisementsResponse, ad_files)

struct EchoRequest : Header {
    std::string message;
    inline MessageType type() const { return MessageType::EchoRequest; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoRequest, message)

struct EchoResponse : Header {
    std::string message;
    inline MessageType type() const { return MessageType::EchoResponse; }
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoResponse, message)

class Protocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, std::span<u8> message, const Handler& handler) {
        switch (MessageType(message_type)) {
            case MessageType::ConnectionRequest: {
                ConnectionRequest deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::KeyExchange: {
                KeyExchange deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::ClientProfile: {
                ClientProfile deserialized{};
                deserialize_cbor_into(deserialized, message);
                // here
                handler(deserialized);
            } break;
            case MessageType::UpdateAvailable: {
                UpdateAvailable deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::StartAdvertising: {
                StartAdvertising deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::StopAdvertising: {
                StopAdvertising deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::AdvertisementsRequest: {
                AdvertisementsRequest deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::AdvertisementsResponse: {
                AdvertisementsResponse deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::EchoRequest: {
                EchoRequest deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
            case MessageType::EchoResponse: {
                EchoResponse deserialized{};
                deserialize_cbor_into(deserialized, message);
                handler(deserialized);
            } break;
        }
    }
};

}  // namespace CrownLinkProtocol