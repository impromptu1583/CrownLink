#pragma once

#include "../shared_common.h"

#include <unistd.h>
#include <iostream>

#define PlatformWindows  1
#define PlatformMac      2
#define PlatformUnix     3

#if defined(_WIN32)
#define Platform PlatformWindows
#elif defined(__APPLE__)
#define Platform PLATFORM_MAC
#else
#define Platform PlatformUnix
#endif


#if Platform == PlatformWindows
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

bool initialize_sockets();
void shutdown_sockets();

class Socket {
public:
    Socket() {
        initialize_sockets();
    }

    ~Socket() {
        shutdown_sockets();
    }
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