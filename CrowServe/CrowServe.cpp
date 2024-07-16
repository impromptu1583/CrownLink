#include "CrowServe.h"

namespace CrowServe {

bool init_sockets(){
#ifdef Windows
    WSADATA WsaData;
    return WSAStartup( MAKEWORD(2,2), &WsaData ) == NO_ERROR;
#else
    return true;
#endif
}

void deinit_sockets(){
#ifdef Windows
    WSACleanup();
#endif
}

void Socket::try_init(std::stop_token stop_token) {
    {
        std::lock_guard lock{m_mutex};
        m_state = SocketState::Connecting;
    }

    addrinfo hints = {};
    addrinfo* address_info = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    using namespace std::chrono_literals;

    // TODO implement some kind of max retries or increase sleep value gradually

    while (!stop_token.stop_requested() && m_state != SocketState::Ready) {
        std::cout << "attempting connection\n";
        if (const auto error = getaddrinfo("127.0.0.1","33377", &hints, &address_info)) {
            // TODO check error and log
            std::this_thread::sleep_for(1s);
            continue;
        }

        addrinfo* result = nullptr;
        for (result = address_info; result; result = result->ai_next) {
            if ((m_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == -1) {
                continue;
            }

            if (connect(m_socket, result->ai_addr, result->ai_addrlen) == -1) {
                std::cout << "conn err\n";
                close(m_socket);
                continue;
            }

            break;
        }

        if (!result) {
            // TODO log
            std::this_thread::sleep_for(1s);
            continue;
        }

        std::cout << "successfully connected\n";
        freeaddrinfo(address_info);

        // set recv timeout to 1.5s. The server sends a keepalive every second so we should only hit this if disconnected.
        timeval timeout = {1, 500000};
        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));

        std::lock_guard lock{m_mutex};
        m_state = SocketState::Ready;
    }
}

}
