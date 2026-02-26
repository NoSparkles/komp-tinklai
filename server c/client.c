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
    SOCKET sockfd;
    char buf[BUFSIZE];
    char sendbuf[BUFSIZE];
    int rv, numbytes;

    // 1. Initialize Winsock
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        fprintf(stderr, "WSAStartup failed\n");
        return 1;
    }

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;     // IPv4 or IPv6
    hints.ai_socktype = SOCK_STREAM;

    if ((rv = getaddrinfo("::1", PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        WSACleanup();
        return 1;
    }

    for (p = res; p != NULL; p = p->ai_next) {
        if ((sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol)) == INVALID_SOCKET) {
            continue;
        }

        if (connect(sockfd, p->ai_addr, (int)p->ai_addrlen) == SOCKET_ERROR) {
            closesocket(sockfd);
            continue;
        }

        break; // Successfully connected
    }

    if (p == NULL) {
        fprintf(stderr, "Client: failed to connect\n");
        freeaddrinfo(res);
        WSACleanup();
        return 2;
    }

    freeaddrinfo(res); // All done with this structure

    printf("Ivesk eilute siuntimui ... --> ");
    if (fgets(sendbuf, BUFSIZE, stdin) != NULL) {
        // Remove newline character if present
        sendbuf[strcspn(sendbuf, "\n")] = 0;

        if (send(sockfd, sendbuf, (int)strlen(sendbuf), 0) == SOCKET_ERROR) {
            fprintf(stderr, "send failed\n");
        } else {
            numbytes = recv(sockfd, buf, BUFSIZE - 1, 0);
            if (numbytes > 0) {
                buf[numbytes] = '\0'; // Null-terminate the string
                printf("Gauta ... %s\n", buf);
            } else if (numbytes == 0) {
                printf("Connection closed by server\n");
            } else {
                fprintf(stderr, "recv failed\n");
            }
        }
    }

    closesocket(sockfd);
    WSACleanup();

    return 0;
}