#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>
#include <netinet/in.h> // Required for IPV6 macros

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int klientu_soketai[MAX_CLIENTS];
int pipes[MAX_CLIENTS][2];

void siusti_visiems(const char *zinute) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (klientu_soketai[i] != -1) {
            send(klientu_soketai[i], zinute, strlen(zinute), 0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Naudojimas: %s <portas>\n", argv[0]);
        return 1;
    }

    int portas = atoi(argv[1]);
    int server_fd, naujas_soketas;
    struct sockaddr_in6 adresas; // Changed to sockaddr_in6
    fd_set readfds;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        klientu_soketai[i] = -1;
        pipes[i][0] = -1;
    }

    // 1. Create an IPv6 socket
    server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket creation failed");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. IMPORTANT: Turn OFF IPV6_V6ONLY to allow IPv4 connections on the same socket
    int no = 0;
    if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0) {
        perror("Failed to set IPV6_V6ONLY to 0");
    }

    // 3. Set up the address structure for IPv6
    memset(&adresas, 0, sizeof(adresas));
    adresas.sin6_family = AF_INET6;
    adresas.sin6_addr = in6addr_any; // This is the IPv6 equivalent of INADDR_ANY
    adresas.sin6_port = htons(portas);

    if (bind(server_fd, (struct sockaddr *)&adresas, sizeof(adresas)) < 0) {
        perror("Bind failed");
        exit(1);
    }

    listen(server_fd, 10);
    printf("Serveris veikia (IPv4 + IPv6) porte %d...\n", portas);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (pipes[i][0] != -1) {
                FD_SET(pipes[i][0], &readfds);
                if (pipes[i][0] > max_fd) max_fd = pipes[i][0];
            }
        }

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            // Accept can still use generic sockaddr
            struct sockaddr_in6 client_addr;
            socklen_t addr_len = sizeof(client_addr);
            naujas_soketas = accept(server_fd, (struct sockaddr *)&client_addr, &addr_len);
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (klientu_soketai[i] == -1) {
                    klientu_soketai[i] = naujas_soketas;
                    pipe(pipes[i]);

                    if (fork() == 0) { // CHILD
                        close(server_fd);
                        close(pipes[i][0]);
                        
                        char buffer[BUFFER_SIZE];
                        char vardas[50];
                        
                        send(naujas_soketas, "ATSIUSKVARDA\n", 13, 0);
                        int n = read(naujas_soketas, buffer, 49);
                        if (n > 0) {
                            buffer[n] = '\0';
                            strtok(buffer, "\r\n");
                            strcpy(vardas, buffer);
                            send(naujas_soketas, "VARDASOK\n", 9, 0);

                            while ((n = read(naujas_soketas, buffer, BUFFER_SIZE)) > 0) {
                                buffer[n] = '\0';
                                strtok(buffer, "\r\n");
                                
                                char full_msg[BUFFER_SIZE + 100];
                                snprintf(full_msg, sizeof(full_msg), "PRANESIMAS %s: %s\n", vardas, buffer);
                                write(pipes[i][1], full_msg, strlen(full_msg));
                            }
                        }
                        close(naujas_soketas);
                        exit(0);
                    } else { // PARENT
                        close(pipes[i][1]);
                        break;
                    }
                }
            }
        }

        // Pipe checking logic remains the same...
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (pipes[i][0] != -1 && FD_ISSET(pipes[i][0], &readfds)) {
                char msg_from_pipe[BUFFER_SIZE + 100];
                int n = read(pipes[i][0], msg_from_pipe, sizeof(msg_from_pipe) - 1);
                if (n <= 0) {
                    close(pipes[i][0]);
                    pipes[i][0] = -1;
                    klientu_soketai[i] = -1;
                } else {
                    msg_from_pipe[n] = '\0';
                    siusti_visiems(msg_from_pipe);
                }
            }
        }
    }
    return 0;
}