#pragma once

#include "common.h"

#include "CrownLinkProtocol.h"
#include "P2PProtocol.h"

#include <unistd.h>
#include <iostream>
#include <functional>
#include <thread>
#include <variant>
#include <span>

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
#else
#include <sys/types.h>
#include <sys/socket.h>
#endif

#include <netdb.h>

namespace CrowServe {

template <typename... Ts>
struct Overloaded : Ts... { using Ts::operator()...; };

enum class ProtocolType : u16 {
    ProtocolCrownLink = 1,
    ProtocolP2P = 2,
};

#pragma pack(push, 1)

struct Header {
    u8 magic[4];
    u16 version;
    u16 protocol;
    u32 message_count;
};

struct MessageHeader {
    u64 message_size;
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

    bool try_init();

    template <typename Handler>
    void listen(const Handler& handler) { // todo: pass stop_token to jthread
        m_thread = std::jthread{[this, handler] {
            while (true) {
                Header main_header{};
                if (receive_into(main_header) < 1) {
                    // TODO: error handling 0 = conn closed, -1 = check errorno
                    std::cout << "error";
                    return;
                }
                
                for (u32 i = 0; i < main_header.message_count; i++) {
                    MessageHeader message_header{};
                    if (receive_into(message_header) < 1) {
                        // TODO: error handling
                        std::cout << "error";
                        return;
                    }

                    std::span<const char> message;

                    if (message_header.message_size > 0) {  
                        char buffer[4096];
                        auto bytes_received = recv(m_socket, buffer, message_header.message_size,0);
                        if (bytes_received < 1) {
                            // TODO: error handling
                            std::cout << "error";
                            return;
                        }
                        message = std::span<const char>{buffer, (u32)bytes_received};
                    }

                    switch (ProtocolType(main_header.protocol)){
                        case ProtocolType::ProtocolCrownLink: {
                            m_crownlink_protocol.handle(message_header.message_type, message, handler);
                        } break;
                        case ProtocolType::ProtocolP2P: {
                            m_p2p_protocol.handle(message_header.message_type, message, handler);
                        } break;
                    }
                }
            }
        }};
    }

    bool receive();

private:
    template<typename T>
    s32 receive_into(T& container) {
        static_assert(std::is_trivial_v<T>);
        auto bytes_remaining = sizeof(container);
        u32 offset = 0;

        while (bytes_remaining > 0) {
            auto bytes_received = recv(m_socket, &container + offset, bytes_remaining, 0);
            if (bytes_received < 1) {
                // currently we back-propagate for error handling
                // TODO: don't expose details of socket implementation to the caller, instead return bool or custom enum,
                //       maybe log error here (we should probably include spdlog?), and just return bool
                return bytes_received;
            }
            bytes_remaining -= bytes_received;
            offset += bytes_received;
        }

        return 1;
    }
    
private:
    u32 m_socket = 0;
    std::jthread m_thread;
    CrownLink::Protocol m_crownlink_protocol;
    P2P::Protocol m_p2p_protocol;
};

}