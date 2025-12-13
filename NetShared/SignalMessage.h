#pragma once
#include "../shared_common.h"
#include "StormTypes.h"

namespace signaling {

enum class MessageType {
	ConnectionRequest,  // body: public_key, info needed for reconnect if need be
	// from here on out all body contents to be encrypted

	ClientProfile,      // server sends client info to connect (STUN/TURN etc)

	StartAdvertising,   // includes game ad as payload
	StopAdvertising,

	Advertisements,     // used for request and response

	ping,               // includes payload peer_id (or blank = ping server) (server sends as keepalive 1/s)
	pong,               // includes payload peer_id (or blank = from server)

	JuiceLocalDescription,
	JuciceCandidate,
	JuiceDone,

	Echo,               // perhaps replaced by ping command
	GamePacket			// potential support of relaying
};

inline std::string to_string(MessageType value) {
	switch (value) {
		EnumStringCase(MessageType::Echo);
	}
	return std::to_string((s32) value);
}


}

struct ConnectionRequest {
	u32 version{0};
	TurnsPerSecond mode{TurnsPerSecond::CNLK};
	NetAddress previous_id{}; // initialize to zero or something if not needed?
};

struct IceServer {
	u32 port{0};
	std::string host{""};
	std::string username{""}; // leave blank for STUN
	std::string password{""};
};

struct ConnectionInfo {
	IceServer stun_server{};
	IceServer turn_server_1{};
	IceServer turn_server_2{};
};

struct ClientProfile {
	NetAddress client_id;
	std::string connection_info;
};