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
    std::string PreSharedKey;
    bool PeerIdRequested;
    NetAddress RequestedId;
    NetAddress RequestToken;
    u32 ProductId;
    u32 VersionId;
    u32 ClinkVersion;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ConnectionRequest, PreSharedKey, PeerIdRequested, RequestedId, RequestToken, ProductId, VersionId, ClinkVersion)

struct KeyExchange : Header {
    std::string PublicKey;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(KeyExchange, PublicKey)

struct TurnServer {
    std::string Host;
    std::string Port;
    std::string Username;
    std::string Password;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(TurnServer, Host, Port, Username, Password)

struct IceCredentials {
    std::string StunServer;
    std::string StunPort;
    TurnServer TurnServers[2];
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(IceCredentials, StunServer, StunPort, TurnServers)

struct ClientProfile : Header {
    NetAddress PeerId;
    NetAddress Token;
    IceCredentials IceCreds;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(ClientProfile, PeerId, Token, IceCreds)

struct UpdateRequested : Header {
    u32 MinClinkVersion;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(UpdateRequested, MinClinkVersion)

struct AdvertisementRequest : Header {
};

// GameInfo and AdFile are defined in StormTypes.h

struct StartAdvertising : Header {
    AdFile Ad;
};

struct StopAdvertising : Header {
};

struct AdvertisementsRequest : Header {
};

struct AdvertisementsResponse : Header {
    std::vector<AdFile> AdFiles;
};

struct EchoRequest : Header {
    std::string Message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoRequest, Message)

struct EchoResponse : Header {
    std::string Message;
};
NLOHMANN_DEFINE_TYPE_NON_INTRUSIVE(EchoResponse, Message)

class CrownLinkProtocol {
public:
    template <typename Handler>
    void handle(const MessageType message_type, const std::span<char> message, const Handler& handler) {

    }
};

}