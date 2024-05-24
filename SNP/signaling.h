#pragma once
#include "common.h"
#include "JuiceManager.h"
#include "config.h"

enum class SignalMessageType {
	StartAdvertising = 1,
	StopAdvertising,
	RequestAdvertisers,
	SolicitAds,
	GameAd,

	JuiceLocalDescription = 101,
	JuciceCandidate,
	JuiceDone,

	ServerSetID = 254,
	ServerEcho = 255,
};

inline std::string to_string(SignalMessageType value) {
	switch (value) {
		EnumStringCase(SignalMessageType::StartAdvertising);
		EnumStringCase(SignalMessageType::StopAdvertising);
		EnumStringCase(SignalMessageType::RequestAdvertisers);
		EnumStringCase(SignalMessageType::SolicitAds);
		EnumStringCase(SignalMessageType::GameAd);

		EnumStringCase(SignalMessageType::JuiceLocalDescription);
		EnumStringCase(SignalMessageType::JuciceCandidate);
		EnumStringCase(SignalMessageType::JuiceDone);

		EnumStringCase(SignalMessageType::ServerSetID);
		EnumStringCase(SignalMessageType::ServerEcho);
	}
	return std::to_string((s32)value);
}

enum class SocketState {
	Uninitialized,
	Initialized,
	Connecting,
	Ready
};

inline std::string to_string(SocketState value) {
	switch (value) {
		EnumStringCase(SocketState::Uninitialized);
		EnumStringCase(SocketState::Initialized);
		EnumStringCase(SocketState::Connecting);
		EnumStringCase(SocketState::Ready);
	}
	return std::to_string((s32)value);
}

struct SignalPacket {
	NetAddress peer_address;
	SignalMessageType message_type;
	std::string data;

	SignalPacket() = default;

	SignalPacket(NetAddress address, SignalMessageType type, std::string data)
		: peer_address{address}, message_type{type}, data{data} {}
	
	SignalPacket(std::string& packet_string) {
		memcpy_s((void*)&peer_address.bytes, sizeof(peer_address.bytes), packet_string.c_str(), sizeof(NetAddress));
		message_type = SignalMessageType((int)packet_string.at(16) - 48);
		data = packet_string.substr(sizeof(NetAddress) + 1);
	}
};

class SignalingSocket {
public:
	SignalingSocket() = default;
	SignalingSocket(SignalingSocket&) = delete;
	SignalingSocket& operator=(SignalingSocket&) = delete;

	~SignalingSocket() {
		deinit();
	}

	bool init();
	void deinit();
	void send_packet(NetAddress destination, SignalMessageType message_type, const std::string& message = "");
	void send_packet(const SignalPacket& packet);
	s32 receive_packets(std::vector<SignalPacket>& incoming_packets);
	void start_advertising();
	void stop_advertising();
	void request_advertisers();
	void echo(const std::string& data);
	void set_client_id(const std::string& id);
	
private:
	void split_into_packets(const std::string& s, std::vector<SignalPacket>& incoming_packets);

private:
	inline static const std::string Delimiter = "-+";

	NetAddress m_server{};
	SocketState m_current_state = SocketState::Uninitialized;
	SOCKET m_socket = 0;
	s32 m_state = 0;
	std::string m_host;
	std::string m_port;
	Logger m_logger{Logger::root(), "SignalingSocket"};
};