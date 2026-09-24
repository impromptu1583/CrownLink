#include "CrowServe.h"
#include <mutex>

namespace CrowServe {

bool init_sockets() {
#ifdef Windows
    WSADATA WsaData;
    return WSAStartup(MAKEWORD(2, 2), &WsaData) == NO_ERROR;
#else
    return true;
#endif
}

void deinit_sockets() {
#ifdef Windows
    WSACleanup();
#endif
}

void Socket::try_init(std::stop_token& stop_token) {
    m_state = SocketState::Connecting;
    m_profile_received = false;
    send_status_callback();

    addrinfo hints = {};
    addrinfo* address_info = nullptr;

    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;
    using namespace std::chrono_literals;

    // TODO implement some kind of max retries or increase sleep value gradually

    while (!stop_token.stop_requested() && m_state != SocketState::Ready) {
        if (m_retry_counter > 0) {
            std::this_thread::sleep_for(m_retry_counter * 1s);
        }

        std::cout << "attempting connection\n";
        try_log("Attempting connection to {}:{}", m_host, m_port);
        if (const auto error = getaddrinfo(m_host.c_str(), m_port.c_str(), &hints, &address_info)) {
            m_retry_counter = m_retry_counter < 3 ? m_retry_counter + 1 : 3;
            continue;
        }

        addrinfo* result = nullptr;
        for (result = address_info; result; result = result->ai_next) {
            if ((m_socket = socket(result->ai_family, result->ai_socktype, result->ai_protocol)) == INVALID_SOCKET) {
                m_retry_counter = m_retry_counter < 3 ? m_retry_counter + 1 : 3;
                continue;
            }

            if (connect(m_socket, result->ai_addr, result->ai_addrlen) == -1) {
                try_log("Error: connection error");
                std::cout << "conn err\n";
                closesocket(m_socket);
                m_retry_counter = m_retry_counter < 3 ? m_retry_counter + 1 : 3;
                continue;
            }

            break;
        }

        if (!result) {
            m_retry_counter = m_retry_counter < 3 ? m_retry_counter + 1 : 3;
            continue;
        }

        m_poll_fd[0].fd = m_socket;
        m_poll_fd[0].events = POLLIN;

        try_log("Successfully connected to matchmaking server");
        std::cout << "successfully connected\n";
        freeaddrinfo(address_info);

        m_state = SocketState::Ready;
        send_status_callback();
        m_retry_counter = 0;
    }
}

void Socket::disconnect() {
    shutdown(m_socket, SD_RECEIVE);
    closesocket(m_socket);
    m_state = SocketState::Disconnected;
    send_status_callback();
}

void Socket::log_socket_error(const char* message, s32 bytes_received, s32 error) {
    if (!bytes_received) {
        try_log("Error: Server terminated connection");
        std::cout << message << "Server terminated connection\n";
        return;
    }

#ifdef Windows
    try_log("Error: Winsock error code: {}", error);
    std::cout << message << "Winsock error code: " << error << " \n";
#else
    std::cout << message << "Socket error received: " << std::strerror(error) << error << " \n";
#endif
    return;
}

void Socket::set_profile(const NetAddress& id, const NetAddress& Token) {
    {
        std::unique_lock lock{m_id_mutex};
        m_id = id;
    }
    m_id_received = true;
    m_reconnect_token = Token;
    m_profile_received = true;
}

void Socket::set_status_callback(StatusCallback callback, void* user_data) {
    {
        std::unique_lock lock{m_callback_mutex};
        m_status_callback = callback;
        m_status_callback_userdata = user_data;
    }
    callback(m_state, m_status_callback_userdata);
}

void Socket::send_status_callback() {
    std::shared_lock lock{m_callback_mutex};
    if (m_status_callback) m_status_callback(m_state, m_status_callback_userdata);
}
}  // namespace CrowServe
