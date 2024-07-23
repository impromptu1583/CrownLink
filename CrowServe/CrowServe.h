#pragma once

#include "common.h"

#include "CrownLinkProtocol.h"
#include "P2PProtocol.h"

#include <iostream>
#include <functional>
#include <thread>
#include <variant>
#include <span>
#include <mutex>

#if defined(_WIN32)
#define Windows
#elif defined(__APPLE__)
#define Mac
#else
#define Unix
#endif

#ifdef Windows
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <Winsock2.h>
#include <ws2tcpip.h>

#else
#include <unistd.h>
#include <netdb.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <errno.h>

using SOCKET = s32;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1

inline s32 closesocket(SOCKET s) {
    return close(s);
}

inline s32 WSAGetLastError() {
    return errno;
}
#endif

namespace CrowServe {

template <typename... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

enum class ProtocolType : u16 {
    ProtocolCrownLink = 1,
    ProtocolP2P = 2,
};

enum class SocketState {
	Disconnected,
	Connecting,
	Ready
};

#pragma pack(push, 1)

struct Header {
    Header() = default;
    Header (u16 protocol, u32 message_count) 
    : protocol(protocol), message_count(message_count) {
        memcpy(magic, "CSRV", 4);
        version = 1; // TODO - store a global version somewhere
    };
    u8 magic[4];
    u16 version;
    u16 protocol;
    u32 message_count;
};

struct MessageHeader {
    u32 message_size;
    u32 message_type;
};

#pragma pack(pop)

bool init_sockets();
void deinit_sockets();

class Socket {
public:
    Socket() {
        init_sockets();
    };
    Socket(Socket&) = delete;
	Socket& operator=(Socket&) = delete;
    ~Socket() {
        deinit_sockets();
    };

    void try_init(std::stop_token stop_token);
    void disconnect();
    void log_socket_error(const char* message, s32 bytes_received, s32 error);

    template <typename... Handlers>
    void listen(Handlers&&... handlers) { // todo: pass stop_token to jthread
        m_thread = std::jthread{[this, handler = Overloaded{std::forward<Handlers>(handlers)...}](std::stop_token stop_token) {
            while (!stop_token.stop_requested()) {
                if (m_state != SocketState::Ready) {
                    try_init(stop_token);
                    std::cout << "init done\n";
                }

                Header main_header{};

                auto [bytes, error] = receive_into(main_header);
                if (!bytes || error) {
                    log_socket_error("Main header error: ", bytes, error);
                    disconnect();
                    continue;
                }

                if (strncmp((const char*)main_header.magic, "CSRV", 4) != 0) {
                    std::cout << "Main header magic byte mismatch, received: " << main_header.magic << " resetting connection";
                    disconnect();
                    continue;
                }
                
                for (u32 i = 0; i < main_header.message_count; i++) {
                    MessageHeader message_header{};
                    auto [bytes, error] = receive_into(message_header);
                    if (!bytes || error) {
                        log_socket_error("Message header error: ", bytes, error);
                        disconnect();
                        break;
                    }

                    std::span<const char> message;

                    if (message_header.message_size > 0) {  
                        char buffer[4096];
                        auto bytes_received = recv(m_socket, buffer, message_header.message_size,0);
                        if (bytes_received == 0 || bytes_received == SOCKET_ERROR) {
                            log_socket_error("Message receive error: ", bytes_received, WSAGetLastError());
                            disconnect();
                            break;
                        }
                        message = std::span<const char>{buffer, (u32)bytes_received};
                    }

                    switch (ProtocolType(main_header.protocol)){
                        case ProtocolType::ProtocolCrownLink: {
                            m_crownlink_protocol.handle(CrownLink::MessageType(message_header.message_type), message, handler);
                        } break;
                        case ProtocolType::ProtocolP2P: {
                            //m_p2p_protocol.handle(P2P::MessageType(message_header.message_type), message, handler);
                        } break;
                    }
                }
            }
        }};
    }

template <typename T>
void send_messages(ProtocolType protocol, T& message) {
    if (m_state != SocketState::Ready) {
        // TODO logging
        return;
    }

    std::lock_guard lock{m_mutex};

    Header header{(u16)protocol, 1};

    std::vector<u8> message_buffer{};
    serialize_cbor(message, message_buffer);

    MessageHeader message_header{(const u32)message_buffer.size(), (u32)message.type()};
    
    auto bytes_sent = send(m_socket, (const char*)&header, sizeof(header), 0);
    bytes_sent = send(m_socket, (const char*)&message_header, sizeof(message_header),0);
    bytes_sent = send(m_socket, (const char*)message_buffer.data(), message_buffer.size(), 0);
    // TODO: combine before sending, check bytes_sent for error
}

private:
    template <typename T>
    std::pair<s32,s32> receive_into(T& container) {
        static_assert(std::is_trivial_v<T>);
        auto bytes_remaining = sizeof(container);
        u32 offset = 0;
        s32 bytes_received = 0;
        while (bytes_remaining > 0) {
            bytes_received = recv(m_socket, (char*)&container + offset, bytes_remaining, 0);
            if (bytes_received == 0 || bytes_received == SOCKET_ERROR) {
                // currently we back-propagate for error handling
                // TODO: don't expose details of socket implementation to the caller, instead return bool or custom enum,
                //       maybe log error here (we should probably include spdlog?), and just return bool
                return {bytes_received, WSAGetLastError()};
            }
            bytes_remaining -= bytes_received;
            offset += bytes_received;
        }

        return {bytes_received, 0};
    }
    
private:
    SOCKET m_socket = 0;
    SocketState m_state = SocketState::Disconnected;
    std::jthread m_thread;
    CrownLink::Protocol m_crownlink_protocol;
    P2P::Protocol m_p2p_protocol;
    std::mutex m_mutex;
};

}