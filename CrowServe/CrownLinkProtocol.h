#pragma once

#include "common.h"
#include "../NetShared/StormTypes.h"

namespace CrownLink {

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

enum class Mode {
    Default,
    DoubleBrainCells,
};

struct Header {};

struct ConnectionRequest : Header {
    std::string preshared_key;
    bool peer_id_requested;
    NetAddress requested_id;
    NetAddress request_token;
    u32 product_id;
    u32 version_id;
    u32 crownlink_version;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConnectionRequest, preshared_key, peer_id_requested, requested_id, request_token, product_id, version_id, crownlink_version)

struct KeyExchange : Header {
    std::string public_key;
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
    TurnServer turn_servers[2];
};
inline void to_json(Json& j, const IceCredentials& ice_credentials) {
    j = Json{{"stun_host", ice_credentials.stun_host},
            {"stun_port", ice_credentials.stun_port}
            // TODO: handle TurnServers
    };
}
inline void from_json(const Json& j, IceCredentials& ice_credentials) {
    j.at("stun_host").get_to(ice_credentials.stun_host);
    j.at("stun_port").get_to(ice_credentials.stun_port);
    if (j["turn_servers"].is_array()) {
        auto servers = j.at("turn_servers");
        for (u32 i = 0; i < 2 && i < servers.size(); i++) {
            ice_credentials.turn_servers[i] = servers[i].template get<TurnServer>();
        }
    }
}

struct ClientProfile : Header {
    NetAddress peer_id;
    NetAddress token;
    IceCredentials ice_credentials;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClientProfile, peer_id, token, ice_credentials)

struct UpdateRequested : Header {
    u32 min_clink_version;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UpdateRequested, min_clink_version)

struct AdvertisementRequest : Header {
};

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising : Header {
    AdFile ad;
};

struct StopAdvertising : Header {
};

struct AdvertisementsRequest : Header {
};

struct AdvertisementsResponse : Header {
    std::vector<AdFile> ad_files;
};

struct EchoRequest : Header {
    std::string message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoRequest, message)

struct EchoResponse : Header {
    std::string message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoResponse, message)

class CrownLinkProtocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<char> message, const Handler& handler) {

    }
};

}