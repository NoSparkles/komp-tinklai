#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/select.h>
#include <netinet/in.h> // Reikalinga IPv6 makrosams

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

typedef struct {
    int socket;
    char vardas[50];
    int vardas_nustatytas;
} Klientas;

Klientas klientai[MAX_CLIENTS];

void siusti_visiems(const char *zinute) {
    for (int i = 0; i < MAX_CLIENTS; i++) {
        if (klientai[i].socket != -1 && klientai[i].vardas_nustatytas) {
            send(klientai[i].socket, zinute, strlen(zinute), 0);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Naudojimas: %s <portas>\n", argv[0]);
        return 1;
    }

    int portas = atoi(argv[1]);
    int server_fd, naujas_soketas, max_fd, activity;
    struct sockaddr_in6 adresas; // Pakeista į sockaddr_in6
    fd_set readfds;
    char buffer[BUFFER_SIZE];

    for (int i = 0; i < MAX_CLIENTS; i++) {
        klientai[i].socket = -1;
        klientai[i].vardas_nustatytas = 0;
    }

    // 1. Sukuriame AF_INET6 lizdą
    server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    if (server_fd < 0) {
        perror("Socket klaida");
        exit(1);
    }

    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    // 2. Išjungiam IPV6_V6ONLY, kad veiktų Dual-Stack (IPv4 + IPv6)
    int no = 0;
    if (setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &no, sizeof(no)) < 0) {
        perror("IPV6_V6ONLY klaida");
    }

    // 3. Konfigūruojame adresą IPv6 formatu
    memset(&adresas, 0, sizeof(adresas));
    adresas.sin6_family = AF_INET6;
    adresas.sin6_addr = in6addr_any; // Atitikmuo INADDR_ANY
    adresas.sin6_port = htons(portas);

    if (bind(server_fd, (struct sockaddr *)&adresas, sizeof(adresas)) < 0) {
        perror("Bind klaida");
        exit(1);
    }

    listen(server_fd, 3);
    printf("Serveris veikia (Dual-Stack IPv4/IPv6) porte %d...\n", portas);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        max_fd = server_fd;

        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (klientai[i].socket > 0) {
                FD_SET(klientai[i].socket, &readfds);
            }
            if (klientai[i].socket > max_fd) {
                max_fd = klientai[i].socket;
            }
        }

        activity = select(max_fd + 1, &readfds, NULL, NULL, NULL);

        if (FD_ISSET(server_fd, &readfds)) {
            struct sockaddr_in6 client_addr;
            socklen_t addrlen = sizeof(client_addr);
            naujas_soketas = accept(server_fd, (struct sockaddr *)&client_addr, &addrlen);
            
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (klientai[i].socket == -1) {
                    klientai[i].socket = naujas_soketas;
                    send(naujas_soketas, "ATSIUSKVARDA\n", 13, 0);
                    break;
                }
            }
        }

        for (int i = 0; i < MAX_CLIENTS; i++) {
            int sd = klientai[i].socket;
            if (sd > 0 && FD_ISSET(sd, &readfds)) {
                int valread = read(sd, buffer, BUFFER_SIZE - 1);
                if (valread <= 0) {
                    close(sd);
                    klientai[i].socket = -1;
                    klientai[i].vardas_nustatytas = 0;
                } else {
                    buffer[valread] = '\0';
                    strtok(buffer, "\r\n");

                    if (!klientai[i].vardas_nustatytas) {
                        strncpy(klientai[i].vardas, buffer, 49);
                        klientai[i].vardas_nustatytas = 1;
                        send(sd, "VARDASOK\n", 9, 0);
                    } else {
                        char broadcast_msg[BUFFER_SIZE + 100];
                        snprintf(broadcast_msg, sizeof(broadcast_msg), "PRANESIMAS %s: %s\n", klientai[i].vardas, buffer);
                        siusti_visiems(broadcast_msg);
                    }
                }
            }
        }
    }
    return 0;
}