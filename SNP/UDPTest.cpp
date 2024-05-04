#include <iostream>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif

#include <winsock2.h>
#include <ws2tcpip.h>

struct SNETADDR {
    BYTE address[16];
};

int main()
{
    // Initialise Winsock DLL
    // See https://beej.us/guide/bgnet/html/#windows 
    WSADATA wsaData;
    // MAKEWORD(1,1) for Winsock 1.1, MAKEWORD(2,0) for Winsock 2.0
    if (WSAStartup(MAKEWORD(1, 1), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed.\n");
        exit(1);
    }

    // Set up connection and send 
    std::string hostname{ "127.0.0.1" };
    uint16_t port = 9999;

    SOCKET sock = ::socket(AF_INET, SOCK_DGRAM, 0);

    sockaddr_in destination;
    destination.sin_family = AF_INET;
    destination.sin_port = htons(port);
    destination.sin_addr.s_addr = inet_addr(hostname.c_str());

    std::cout << sizeof(destination) << "\n";

    SNETADDR test;
    std::cout << destination.sin_port << "\n";
    std::cout << destination.sin_addr.s_addr << "\n";

    memcpy(&(test), &destination, sizeof(destination));
    
    memcpy(&(destination), &test, sizeof(test));
    std::cout << destination.sin_port << "\n";
    std::cout << destination.sin_addr.s_addr << "\n";

    std::string msg = "ini|";
    int n_bytes = ::sendto(sock, msg.c_str(), msg.length(), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));

    msg = "adv|1";
    n_bytes += ::sendto(sock, msg.c_str(), msg.length(), 0, reinterpret_cast<sockaddr*>(&destination), sizeof(destination));



    std::cout << n_bytes << " bytes sent" << std::endl;
    ::closesocket(sock);

    // Clean up sockets library
    WSACleanup();
}