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

bool Socket::try_init() {  
    addrinfo hints = {};
    addrinfo* result = nullptr;
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    std::cout << "attempting connection\n";
    if (const auto error = getaddrinfo("127.0.0.1","33377", &hints, &result)) {
        return false;
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
		return false;
	}
	freeaddrinfo(result);

    // set recv timeout to 1.5s. The server sends a keepalive every second so we should only hit this if disconnected.
    struct timeval timeout = {};
    timeout.tv_sec = 1;
    timeout.tv_usec = 500000;

    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));
    return true;
}

bool Socket::receive() {
    u8 iterations = 0;
    while (iterations < 10) {
        Header main_header{};
        if (receive_into(main_header) < 1) {
            // TODO: error handling 0 = conn closed, -1 = check errorno
            std::cout << "error";
            return false;
        }
        std::cout << "received main header, protocol: " << main_header.protocol << " count: " << main_header.message_count << " magic:" << main_header.magic << "\n";

        // todo: check magic
        for (auto i = 0; i < main_header.message_count; i++) {
            MessageHeader message_header{};
            if (receive_into(message_header) < 1) {
                // TODO: error handling 0 = conn closed, -1 = check errorno
                std::cout << "error";
                return false;
            }
            std::cout << "received message header, type: " << message_header.message_type << " size: " << message_header.message_size << "\n";
            // TODO: figure out protocol-specific message handling
            char buffer[1600];
            auto p = &buffer;
            auto received = recv(m_socket, &buffer, message_header.message_size, 0);
            auto sp = std::span<const char>{buffer, (u32)received};
            
            Json j = Json::from_cbor(sp);
            std::cout << std::setw(2) << j << std::endl;
            auto teststruct = j.template get<CrownLink::EchoRequest>();
            std::cout << "received message " << teststruct.Message << std::endl;

        }

        iterations++;
        std::cout << iterations;
    }

    return true;
}

}
