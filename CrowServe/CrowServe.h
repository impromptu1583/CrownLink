#pragma once

#include <functional>
#include <iostream>
#include <mutex>
#include <span>
#include <thread>
#include <variant>

#include "CrownLinkProtocol.h"
#include "P2PProtocol.h"
#include "common.h"

#if defined(_WIN32)
#define Windows
#elif defined(__APPLE__)
#define Mac
#else
#define Unix
#endif

#ifdef Windows
#define WIN32_LEAN_AND_MEAN
#include <Winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>

#else
#include <errno.h>
#include <netdb.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

using SOCKET = s32;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SD_RECEIVE SHUT_RDWR

inline s32 closesocket(SOCKET s) {
    return close(s);
}

inline s32 WSAGetLastError() {
    return errno;
}
#endif

namespace CrowServe {

template <typename... Ts>
struct Overloaded : Ts... {
    using Ts::operator()...;
};

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

    Header(u16 protocol, u32 message_count) : protocol{protocol}, message_count{message_count}, version{CL_VERSION_NUMBER} {
        memcpy(magic, "CSRV", sizeof(magic));
    };

    u8 magic[4];
    u32 version;
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
    using Logger = std::function<void(const std::string&)>;

    Socket() { init_sockets(); };

    Socket(const Socket&) = delete;
    Socket(Socket&&) = delete;
    Socket& operator=(const Socket&) = delete;
    Socket& operator=(Socket&&) = delete;

    ~Socket() {
        disconnect();
        deinit_sockets();
    };

    void try_init(std::stop_token& stop_token);
    void disconnect();
    void log_socket_error(const char* message, s32 bytes_received, s32 error);
    void set_profile(NetAddress ID, NetAddress Token);
    SocketState state() { return m_state; }
    NetAddress& id() { return m_id; }

    template <typename... Args>
    void try_log(const std::string& message, Args&&... args) const {
        if (m_external_logger) {
            m_external_logger(std::vformat(message, std::make_format_args(args...)));
        } else {
            std::cout << std::vformat(message, std::make_format_args(args...)) << std::endl;
        }
    }

    template <typename... Handlers>
    std::jthread listen(
        const std::string& host, const std::string& port, const std::string& lobby_password, Handlers&&... handlers
    ) {
        m_host = host;
        m_port = port;
        m_lobby_password = lobby_password;
        return std::jthread{[this,
                             handler = Overloaded{std::forward<Handlers>(handlers)...}](std::stop_token stop_token) {
            while (!stop_token.stop_requested()) {
                if (m_state != SocketState::Ready) {
                    try_init(stop_token);
                    try_log("init done");
                    std::cout << "init done\n";
                }

                if (!m_profile_received) {
                    auto request = CrownLinkProtocol::ConnectionRequest{};
                    request.preshared_key = "";
                    request.peer_id_requested = false;
                    request.requested_id = NetAddress{0};
                    request.request_token = NetAddress{0};
                    request.lobby_password = m_lobby_password;
                    request.product_id = 0;
                    request.version_id = 0;
                    request.crownlink_version = 0;
                    if (m_id_received) {
                        request.peer_id_requested = true;
                        request.requested_id = m_id;
                        request.request_token = m_reconnect_token;
                    }
                    send_messages(ProtocolType::ProtocolCrownLink, request);
                }

                Header main_header{};

                auto poll_ready_count = WSAPoll(m_poll_fd, 1, 100);
                if (poll_ready_count == SOCKET_ERROR) {
                    try_log("Winsock error while polling: {}", WSAGetLastError());
                    disconnect();
                    continue;
                } else if (!poll_ready_count && m_profile_received) {
                    auto time_since_message = std::chrono::steady_clock::now() - m_last_message;
                    if (time_since_message > 1.5s) {
                        auto missed = time_since_message / 1s;
                        if (missed != m_missed_heartbeats) {
                            m_missed_heartbeats = missed;
                            try_log("Missed {} server heartbeats", m_missed_heartbeats);
                        }
                        if (m_missed_heartbeats > 5) {
                            m_last_message = std::chrono::steady_clock::now();
                            m_missed_heartbeats = 0;
                            disconnect();
                        }
                    }
                    continue;
                }
                m_last_message = std::chrono::steady_clock::now();
                m_missed_heartbeats = 0;

                auto [bytes, error] = receive_into(main_header);
                if (!bytes || error) {
                    log_socket_error("Main header error: ", bytes, error);
                    disconnect();
                    continue;
                }

                if (strncmp((const char*)main_header.magic, "CSRV", 4) != 0) {
                    try_log("Error: Main header mismatched magic byte received, resetting connection");
                    std::cout << "Main header magic byte mismatch, received: " << main_header.magic
                              << " resetting connection";
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

                    std::span<u8> message;

                    if (message_header.message_size > 0) {
                        u8 buffer[4096];
                        auto bytes_received = recv(m_socket, (char*)buffer, message_header.message_size, 0);
                        if (bytes_received == 0 || bytes_received == SOCKET_ERROR) {
                            log_socket_error("Message receive error: ", bytes_received, WSAGetLastError());
                            disconnect();
                            break;
                        }
                        message = std::span<u8>{buffer, (u32)bytes_received};
                    }

                    switch (ProtocolType(main_header.protocol)) {
                        case ProtocolType::ProtocolCrownLink: {
                            m_crownlink_protocol.handle(
                                CrownLinkProtocol::MessageType(message_header.message_type), message, handler
                            );
                        } break;
                        case ProtocolType::ProtocolP2P: {
                            m_p2p_protocol.handle(P2P::MessageType(message_header.message_type), message, handler);
                        } break;
                    }
                }
            }
        }};
    }

    template <typename T>
    bool send_messages(ProtocolType protocol, T& message) {
        std::lock_guard lock{m_mutex};

        if (m_state != SocketState::Ready) {
            try_log("Error: attempting to send to server but connection isn't ready");
            std::cout << "socket not ready" << std::endl;
            return false;
        }

        Header header{(u16)protocol, 1};

        std::vector<u8> message_buffer{};
        serialize_cbor(message, message_buffer);

        MessageHeader message_header{(const u32)message_buffer.size(), (u32)message.type()};
        auto bytes_sent = send(m_socket, (const char*)&header, sizeof(header), 0);
        bytes_sent = send(m_socket, (const char*)&message_header, sizeof(message_header), 0);
        bytes_sent = send(m_socket, (const char*)message_buffer.data(), message_buffer.size(), 0);
        // TODO: combine before sending, check bytes_sent for error

        return true;
    }

    void set_external_logger(Logger logger) { m_external_logger = std::move(logger); }

private:
    template <typename T>
    std::pair<s32, s32> receive_into(T& container) {
        static_assert(std::is_trivial_v<T>);
        auto bytes_remaining = sizeof(container);
        u32 offset = 0;
        s32 bytes_received = 0;
        u8 allowed_timeouts = 10;  // 1s
        while (bytes_remaining > 0) {
            auto poll_ready_count = WSAPoll(m_poll_fd, 1, 100);
            if (poll_ready_count == SOCKET_ERROR) {
                return {bytes_received, WSAGetLastError()};
            } else if (!poll_ready_count) {
                if (allowed_timeouts) {
                    allowed_timeouts--;
                    continue;
                } else {
                    return {bytes_received, WSAETIMEDOUT};
                }
            }

            bytes_received = recv(m_socket, (char*)&container + offset, bytes_remaining, 0);
            if (bytes_received == 0 || bytes_received == SOCKET_ERROR) {
                // currently we back-propagate for error handling
                // TODO: don't expose details of socket implementation to the caller, instead return bool or custom
                // enum,
                //       maybe log error here (we should probably include spdlog?), and just return bool
                return {bytes_received, WSAGetLastError()};
            }
            bytes_remaining -= bytes_received;
            offset += bytes_received;
        }

        return {bytes_received, 0};
    }

private:
    bool m_profile_received = false;
    Logger m_external_logger;

    SOCKET m_socket = 0;
    WSAPOLLFD m_poll_fd[1] = {0};
    s64 m_missed_heartbeats = 0;
    std::chrono::steady_clock::time_point m_last_message = std::chrono::steady_clock::now();

    NetAddress m_id;
    NetAddress m_reconnect_token;
    bool m_id_received = false;
    u32 m_retry_counter = 0;

    std::string m_host = "127.0.0.1";
    std::string m_port = "33377";
    std::string m_lobby_password = "";
    SocketState m_state = SocketState::Disconnected;
    CrownLinkProtocol::Protocol m_crownlink_protocol;
    P2P::Protocol m_p2p_protocol;
    std::mutex m_mutex;
};

}  // namespace CrowServe