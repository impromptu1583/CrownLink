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

void Socket::try_init(std::stop_token &stop_token) {
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
        if (const auto error = getaddrinfo(m_host.c_str(), m_port.c_str(), &hints, &address_info)) {
            // TODO check error and log
            std::this_thread::sleep_for(1s);
            continue;
        }

        addrinfo* result = nullptr;
        for (result = address_info; result; result = result->ai_next) {
            if ((m_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == INVALID_SOCKET) {
                continue;
            }

            if (connect(m_socket, result->ai_addr, result->ai_addrlen) == -1) {
                std::cout << "conn err\n";
                closesocket(m_socket);
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

        std::lock_guard lock{m_mutex};
        m_state = SocketState::Ready;
    }
}

void Socket::disconnect() {
    shutdown(m_socket, SD_RECEIVE);
    closesocket(m_socket);
    m_state = SocketState::Disconnected;
}

void Socket::log_socket_error(const char* message, s32 bytes_received, s32 error) {
    if (!bytes_received) {
        std::cout << message << "Server terminated connection\n";
        return;
    }
    
#ifdef Windows
    std::cout << message << "Winsock error code: " << error << " \n";
#else
    std::cout << message << "Socket error received: " << std::strerror(error) << error << " \n";
#endif
    return;
}

}
