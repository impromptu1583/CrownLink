#pragma once

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

struct KeyExchange : Header {
    std::string public_key;
};

struct TurnServer {
    std::string host;
    std::string port;
    std::string username;
    std::string password;
};

struct IceCredentials {
    std::string stun_host;
    std::string stun_port;
    TurnServer turn_servers[2];
};

struct ClientProfile : Header {
    NetAddress peer_id;
    NetAddress request_token;
    IceCredentials ice_credentials;
};

struct UpdateRequested : Header {
    u32 minimum_version;
};

struct AdvertisementRequest : Header {
};

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising {
    Header header;
    AdFile ad_file;
};

struct StopAdvertising : Header {
};

struct AdvertisementsRequest : Header {
};

struct AdvertisementsResponse : Header {
    std::vector<AdFile> ads;
};

struct EchoRequest : Header {
    std::string message;
};

struct EchoResponse {
    Header header;
    
    std::string message;
};

class CrownLinkProtocol {
public:
    template <typename Handler>
    void handle(const std::span<char> message, const Handler& handler) {
        
    }
};

}