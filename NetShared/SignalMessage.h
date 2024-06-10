#include "../shared_common.h"

namespace signaling {

enum class MessageType {
	ConnectionRequest,  // client sends to server to request connection (including requests like reconnect same id)
	Challenge,          // server sends a challenge to verify connection
	ChallengeResponse,  // client sends response
	ClientProfile,      // server sends client info to connect (STUN/TURN etc)

	StartAdvertising,   // includes game ad as payload
	StopAdvertising,

	Advertisements,     // used for request and response

	ping,               // includes payload peer_id (or blank = ping server) (server sends as keepalive 1/s)
	pong,               // includes payload peer_id (or blank = from server)

	JuiceLocalDescription,
	JuciceCandidate,
	JuiceDone,
	JuiceTurnCredentials,

	Echo,               // perhaps replaced by ping command
	GamePacket			// potential support of relaying
};

inline std::string to_string(MessageType value) {
	switch (value) {
		EnumStringCase(MessageType::ConnectionRequest);
		EnumStringCase(MessageType::Challenge);
		EnumStringCase(MessageType::ChallengeResponse);
		EnumStringCase(MessageType::ClientProfile);
		EnumStringCase(MessageType::StartAdvertising);
		EnumStringCase(MessageType::StopAdvertising);
		EnumStringCase(MessageType::Advertisements);
		EnumStringCase(MessageType::ping);
		EnumStringCase(MessageType::pong);
		EnumStringCase(MessageType::JuiceLocalDescription);
		EnumStringCase(MessageType::JuciceCandidate);
		EnumStringCase(MessageType::JuiceDone);
		EnumStringCase(MessageType::JuiceTurnCredentials);
		EnumStringCase(MessageType::Echo);

	}
	return std::to_string((s32) value);
}


}

