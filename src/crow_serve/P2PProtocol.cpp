#include "P2PProtocol.h"

namespace P2P {
std::ostream& operator<<(std::ostream& out, MessageType message_type) {
    switch (message_type) {
        case MessageType::ConnectionRequest:
            return out << "ConnectionRequest";
        case MessageType::Pong:
            return out << "Pong";
        case MessageType::JuiceLocalDescription:
            return out << "Juice Local Description";
        case MessageType::JuiceCandidate:
            return out << "Juice Candidate";
        case MessageType::JuiceDone:
            return out << "Juice Done";
    }
    return out;
}
}  // namespace P2P