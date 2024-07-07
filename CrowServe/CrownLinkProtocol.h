#pragma once
#include "CrowServe.h"

#include "../NetShared/StormTypes.h"

enum class CrownLinkMessageType {
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

enum class CrownLinkMode {
    Default,
    DoubleBrainCells,
};

struct Header {};

struct ConnectionRequest {
    Header header;
    std::string preshared_key;
    bool peer_id_requested;
    NetAddress requested_id;
    NetAddress request_token;
    u32 product_id;
    u32 version_id;
    u32 crownlink_version;
};

struct KeyExchange {
    Header header;
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

struct ClientProfile {
    Header header;
    NetAddress peer_id;
    NetAddress request_token;
    IceCredentials ice_credentials;
};

struct UpdateRequested {
    Header header;
    u32 minimum_version;
};

struct AdvertisementRequeste {
    Header header;
};

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising {
    Header header;
    AdFile ad_file;
};

struct StopAdvertising {
    Header header;
};

struct AdvertisementsRequest {
    Header header;
};

struct AdvertisementsResponse {
    Header header;

    AdFile ads[];
};

struct EchoRequest {
    Header header;
    std::string message;
};

struct EchoResponse {
    Header header;
    std::string message;
};