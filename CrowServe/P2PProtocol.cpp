#include "P2PProtocol.h"    

namespace P2P {

std::ostream& operator<<(std::ostream& out, const MessageType& message_type){
    switch (message_type)
    {
        case MessageType::Ping: return out << "Ping";
        case MessageType::Pong: return out << "Pong";
        case MessageType::JuiceLocalDescription: return out << "Juice Local Description";
        case MessageType::JuiceCandidate: return out << "Juice Candidate";
        case MessageType::JuiceDone: return out << "Juice Done";
    }
    return out;
}

}