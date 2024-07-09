#pragma once

#include "../shared_common.h"

#include <unistd.h>
#include <iostream>

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


#pragma pack(push, 1)

namespace CrowServe {

enum class Protocol : u16 {
    ProtocolCrownLink = 1,
    ProtocolP2P = 2,
};

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
    void send();
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
};



}