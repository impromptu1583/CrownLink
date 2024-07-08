#include "CrowServe.hpp"

bool temp_test() {
    
    return true;
}

void try_init() {

}

void CrowServeSocket::receive () {
    static constexpr unsigned int MAX_BUF_LENGTH = 4096;
	std::vector<char> buffer(MAX_BUF_LENGTH);

    auto bytes = recv(m_socket, &buffer[0], buffer.size(), 0);

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

    for (auto info = result; info; info->ai_next) {
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

    return true;
}
