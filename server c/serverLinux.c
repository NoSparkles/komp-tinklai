#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>

#define PORT "20000"
#define BUFSIZE 1024

int main(void) {
    struct addrinfo hints, *res, *p;
    int sockfd, new_fd;
    char buf[BUFSIZE];
    int rv, numbytes;
    int yes = 1;

    memset(&hints, 0, sizeof hints);
    hints.ai_family = AF_UNSPEC;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_PASSIVE;

    if ((rv = getaddrinfo("::1", PORT, &hints, &res)) != 0) {
        fprintf(stderr, "getaddrinfo: %s\n", gai_strerror(rv));
        exit(1);
    }

    for (p = res; p != NULL; p = p->ai_next) {
        sockfd = socket(p->ai_family, p->ai_socktype, p->ai_protocol);
        if (sockfd == -1) continue;

        setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(int));

        if (bind(sockfd, p->ai_addr, p->ai_addrlen) == -1) {
            close(sockfd);
            continue;
        }
        break;
    }

    freeaddrinfo(res);

    if (p == NULL) {
        fprintf(stderr, "server: failed to bind\n");
        exit(2);
    }

    listen(sockfd, 1);
    printf("Serveris laukia (Linux)...\n");

    new_fd = accept(sockfd, NULL, NULL);
    if (new_fd == -1) {
        perror("accept");
        exit(3);
    }
    printf("Klientas prisijunge\n");

    while ((numbytes = recv(new_fd, buf, BUFSIZE, 0)) > 0) {
        printf("Gauta: %.*s\n", numbytes, buf);

        for (int i = 0; i < numbytes; i++) {
            if (buf[i] >= 'a' && buf[i] <= 'z')
                buf[i] -= 32;
        }

        send(new_fd, buf, numbytes, 0);
    }

    close(new_fd);
    close(sockfd);
    
    return 0;
}