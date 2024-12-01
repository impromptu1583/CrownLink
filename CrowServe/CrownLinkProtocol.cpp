#include "CrownLinkProtocol.h"

namespace CrownLinkProtocol {

std::ostream& operator<<(std::ostream& out, const MessageType& message_type) {
    switch (message_type) {
        case MessageType::ConnectionRequest:
            return out << "ConnectionRequest";
        case MessageType::KeyExchange:
            return out << "KeyExchange";
        case MessageType::ClientProfile:
            return out << "ClientProfile";
        case MessageType::UpdateAvailable:
            return out << "UpdateAvailable";
        case MessageType::StartAdvertising:
            return out << "StartAdvertising";
        case MessageType::StopAdvertising:
            return out << "StopAdvertising";
        case MessageType::AdvertisementsRequest:
            return out << "AdvertisementsRequest";
        case MessageType::AdvertisementsResponse:
            return out << "AdvertisementsResponse";
        case MessageType::EchoRequest:
            return out << "EchoRequest";
        case MessageType::EchoResponse:
            return out << "EchoResponse";
    }
    return out;
}

}  // namespace CrownLinkProtocol