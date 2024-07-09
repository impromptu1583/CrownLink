#include "CrowServe.hpp"

bool temp_test() {
    
    return true;
}

void try_init() {

}

bool CrowServeSocket::try_init() {  
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
    struct timeval timeout;
    timeout.tv_sec = 1;
    timeout.tv_usec = 500000;

    setsockopt(m_socket, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof(timeout));



    return true;
}



bool CrowServeSocket::receive() {
    u8 iterations = 0;

    while (iterations < 10) {
        CrowServeHeader main_header{};
        if (receive_into(main_header) < 1) {
            // todo error handling 0 = conn closed, -1 = check errorno
            std::cout << "error";
            return false;
        }
        std::cout << "received main header, protocol: " << main_header.protocol << " count: " << main_header.message_count << "\n";
        iterations++;
        std::cout << iterations;
    }
    return true;

}


