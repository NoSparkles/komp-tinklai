#define _WIN32_WINNT 0x0600

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <winsock2.h>
#include <ws2tcpip.h>

#pragma comment(lib, "ws2_32.lib")

#define PORT "20000"
#define BUFSIZE 1024

int main(void) {
    WSADATA wsaData;
    struct addrinfo hints, *res, *p;
    SOCKET sockfd, new_fd;
    char buf[BUFSIZE];
    int rv, numbytes;

    if (WSAStartup(MAKEWORD(2,2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        exit(1);
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;     // use my IP

    if ((rv = getaddrinfo("::1", PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %d\n", rv);
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == INVALID_SOCKET) continue;

        if (bind(sockfd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        WSACleanup();
        exit(2);
    }

    listen(sockfd, 1);
    printf("Serveris laukia...\n");

    new_fd = accept(sockfd, NULL, NULL);
    printf("Klientas prisijunge\n");

    while ((numbytes = recv(new_fd, buf, BUFSIZE, 0)) > 0) {
        printf("Gauta: %.*s\n", numbytes, buf);

        for (int i = 0; i < numbytes; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z')
                buf[i] -= 32;
        }

        send(new_fd, buf, numbytes, 0);
    }

    closesocket(new_fd);
    closesocket(sockfd);
    WSACleanup();
    return 0;
}