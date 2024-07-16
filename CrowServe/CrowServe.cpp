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
    m_mutex.lock();
    m_state = SocketState::Connecting;
    m_mutex.unlock();

    addrinfo hints = {};
    addrinfo* result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    using namespace std::chrono_literals;

    // TODO implement some kind of max retries or increase sleep value gradually

    while (!stop_token.stop_requested() && m_state != SocketState::Ready) {
        std::cout << "attempting connection\n";
        if (const auto error = getaddrinfo("127.0.0.1","33377", &hints, &result)) {
            // TODO check error and log
            std::this_thread::sleep_for(1s);
            continue;
        }

        for (auto info = result; info; info = info->ai_next) {
            if ((m_socket = socket(info->ai_family, info->ai_socktype, info->ai_protocol)) == -1) {
                continue;
            }

            if (connect(m_socket, info->ai_addr, info->ai_addrlen) == -1) {
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
        freeaddrinfo(result);

        // set recv timeout to 1.5s. The server sends a keepalive every second so we should only hit this if disconnected.
        struct timeval timeout = {};
        timeout.tv_sec = 1;
        timeout.tv_usec = 500000;

        setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
        std::lock_guard{m_mutex};
        m_state = SocketState::Ready;
    }
}

}
