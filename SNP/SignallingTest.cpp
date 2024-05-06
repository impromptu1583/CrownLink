
#include <iostream>
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <errno.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#ifndef __BLIZZARD_STORM_HEADER
struct SNETADDR {
	BYTE address[16];
};
#endif

SOCKET sockfd;
struct addrinfo hints, *res, *p;
SNETADDR server;
int errno;


void init();
void sendsvr(SNETADDR& dest, std::string msg);
void recvsvr();



int main(int argc, char* argv[])
{
	init();

	std::string msg;
	msg += reinterpret_cast<const char*>(server.address);
	msg += 1;

	sendsvr(server, msg);
	recvsvr();
	msg = reinterpret_cast<const char*>(server.address);
	msg += 3;
	sendsvr(server, msg);
	recvsvr();

	recvsvr();
	closesocket(sockfd);


}



void init()
{
	//int sockfd;
	int rv;
	char ipstr[INET6_ADDRSTRLEN];
	memset(&hints, 0, sizeof hints);
	hints.ai_family = AF_INET;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	if ((rv = getaddrinfo("localhost", "9999", &hints, &res)) != 0) {
		fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
		return;
	}

	for (p = res; p != NULL; p = p->ai_next) {
		void* addr;
		char* ipver;
		if ((sockfd = socket(p->ai_family, p->ai_socktype,
			p->ai_protocol)) == -1) {
			perror("client: socket");
			continue;
		}

		if (connect(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
			closesocket(sockfd);
			perror("client: connect");
			continue;
		}

		//-------------
		if (p->ai_family == AF_INET) { // IPv4
			struct sockaddr_in* ipv4 = (struct sockaddr_in*)p->ai_addr;
			addr = &(ipv4->sin_addr);
			ipver = "IPv4";
		}
		else { // IPv6
			struct sockaddr_in6* ipv6 = (struct sockaddr_in6*)p->ai_addr;
			addr = &(ipv6->sin6_addr);
			ipver = "IPv6";
		}

		// convert the IP to a string and print it:
		inet_ntop(p->ai_family, addr, ipstr, sizeof ipstr);
		printf("  %s: %s\n", ipver, ipstr);

		//-------------

		break;
	}
	if (p == NULL) {
		fprintf(stderr, "client: failed to connect\n");
		return;
	}

	freeaddrinfo(res);

	//u_long mode = 1; //enable non-blocking
	//ioctlsocket(sockfd, FIONBIO, &mode);
	// need to add error checking

	// server address is 1111111111111111
	memset(&server, 255, sizeof server);

}

void sendsvr(SNETADDR &dest, std::string msg) {
	int n_bytes = send(sockfd, msg.c_str(), msg.length(), 0);
	if (n_bytes == -1) perror("send error");
	std::cout << n_bytes << " bytes sent" << std::endl;
}

void recvsvr() {
	char buff[1200];
	int n_bytes = recv(sockfd, buff, sizeof(buff), 0);
	if (n_bytes == -1) perror("recv error");
	std::cout << n_bytes << " bytes received" << std::endl;
	buff[n_bytes] = '\0';
	std::cout << buff << std::endl;

	int i;
	for (i = 0; i < n_bytes; i++)
	{
		if ((i-1) % 16 == 0) printf("\n");
		if (i > 0) printf(":");
		printf("%02hhX", buff[i]);
	}
	printf("\n");
}


