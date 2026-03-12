#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/select.h>

#define MAX_CLIENTS 30
#define BUFFER_SIZE 1024

int klientu_soketai[MAX_CLIENTS];
int pipes[MAX_CLIENTS][2]; // Vamzdžiai komunikacijai: vaikas -> tėvas

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
    struct sockaddr_in adresas;
    fd_set readfds;

    for (int i = 0; i < MAX_CLIENTS; i++) {
        klientu_soketai[i] = -1;
        pipes[i][0] = -1;
    }

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    adresas.sin_family = AF_INET;
    adresas.sin_addr.s_addr = INADDR_ANY;
    adresas.sin_port = htons(portas);

    bind(server_fd, (struct sockaddr *)&adresas, sizeof(adresas));
    listen(server_fd, 10);

    printf("Serveris veikia porte %d...\n", portas);

    while (1) {
        FD_ZERO(&readfds);
        FD_SET(server_fd, &readfds);
        int max_fd = server_fd;

        // Tėvas stebi visus vamzdžius iš vaikų
        for (int i = 0; i < MAX_CLIENTS; i++) {
            if (pipes[i][0] != -1) {
                FD_SET(pipes[i][0], &readfds);
                if (pipes[i][0] > max_fd) max_fd = pipes[i][0];
            }
        }

        select(max_fd + 1, &readfds, NULL, NULL, NULL);

        // 1. Naujas klientas jungiasi
        if (FD_ISSET(server_fd, &readfds)) {
            naujas_soketas = accept(server_fd, NULL, NULL);
            for (int i = 0; i < MAX_CLIENTS; i++) {
                if (klientu_soketai[i] == -1) {
                    klientu_soketai[i] = naujas_soketas;
                    pipe(pipes[i]); // Sukuriam vamzdį šiam klientui

                    if (fork() == 0) { // VAIKAS
                        close(server_fd);
                        close(pipes[i][0]); // Vaikas tik rašo
                        
                        char buffer[BUFFER_SIZE];
                        char vardas[50];
                        
                        // Protokolas
                        send(naujas_soketas, "ATSIUSKVARDA\n", 13, 0);
                        int n = read(naujas_soketas, buffer, 49);
                        buffer[n] = '\0';
                        strtok(buffer, "\r\n");
                        strcpy(vardas, buffer);
                        send(naujas_soketas, "VARDASOK\n", 9, 0);

                        while ((n = read(naujas_soketas, buffer, BUFFER_SIZE)) > 0) {
                            buffer[n] = '\0';
                            strtok(buffer, "\r\n");
                            
                            char full_msg[BUFFER_SIZE + 100];
                            snprintf(full_msg, sizeof(full_msg), "PRANESIMAS %s: %s\n", vardas, buffer);
                            
                            // Siunčiame tėvui per vamzdį
                            write(pipes[i][1], full_msg, strlen(full_msg));
                        }
                        close(naujas_soketas);
                        exit(0);
                    } else { // TĖVAS
                        close(pipes[i][1]); // Tėvas tik skaito
                        break;
                    }
                }
            }
        }

        // 2. Tėvas gavo žinutę iš bet kurio vaiko per vamzdį
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
                    siusti_visiems(msg_from_pipe); // Tėvas išsiunčia visiems!
                }
            }
        }
    }
    return 0;
}