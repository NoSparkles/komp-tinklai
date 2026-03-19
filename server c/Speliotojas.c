#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <errno.h>

#define BUFSIZE 1024

void set_nonblocking(int fd) {
    int flags = fcntl(fd, F_GETFL, 0);
    fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

// Pagalbinė funkcija nusiųsti žinutę visiems prisijungusiems
void broadcast(int clients[], const char *msg) {
    for (int i = 0; i < 4; i++) {
        if (clients[i] != -1) {
            send(clients[i], msg, strlen(msg), 0);
        }
    }
}

int main(void) {
    int ports[4] = {20000, 20001, 20002, 20003};
    int server_fds[4];
    int client_fds[4] = {-1, -1, -1, -1};
    int target_num = -1;
    int current_turn = 1; 

    FILE *outputFile;

    for (int i = 0; i < 4; i++) {
        server_fds[i] = socket(AF_INET6, SOCK_STREAM, 0);
        int opt = 1;
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
        
        int v6only = 0;
        setsockopt(server_fds[i], IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(ports[i]);
        addr.sin6_addr = in6addr_any;

        if (bind(server_fds[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            perror("bind");
            exit(1);
        }
        listen(server_fds[i], 5);
        set_nonblocking(server_fds[i]);
        printf("Portas %d paruostas.\n", ports[i]);
    }

    while (1) {
        outputFile = fopen("logs.txt", "a");
        for (int i = 0; i < 4; i++) {
            // Naujų ryšių priėmimas
            int new_fd = accept(server_fds[i], NULL, NULL);
            if (new_fd != -1) {
                set_nonblocking(new_fd);
                if (client_fds[i] != -1) close(client_fds[i]);
                client_fds[i] = new_fd;
                
                if (i == 0) {
                    send(new_fd, "Sveikas, Vadove. Ivesk skaiciu (0-10):\n", 39, 0);
                    fprintf(outputFile, "Vadovui siusta: Sveikas, Vadove. Ivesk skaiciu (0-10):\n");
                }
                else {
                    send(new_fd, "Sveikas, Zaidejau. Lauk savo eiles...\n", 38, 0);
                    fprintf(outputFile, "Zaidejui siusta: Sveikas, Vadove. Ivesk skaiciu (0-10):\n");
                }
            }

            // Duomenų gavimas
            if (client_fds[i] != -1) {
                char buf[BUFSIZE];
                int n = recv(client_fds[i], buf, BUFSIZE - 1, 0);

                if (n > 0) {
                    buf[n] = '\0';
                    int val = atoi(buf);

                    if (i == 0) { // VADOVAS
                        target_num = val;
                        current_turn = 1; // Pradeda 1 žaidėjas
                        broadcast(client_fds, "\nZaidimas prasidejo! Vadovas nustate skaiciu.\n");
                        fprintf(outputFile, "Visiems siusta: \nZaidimas prasidejo! Vadovas nustate skaiciu.\n");
                        if (client_fds[current_turn] != -1) 
                            send(client_fds[current_turn], "Tavo eile speti: ", 17, 0);
                    } 
                    else if (target_num != -1 && i == current_turn) { // ŽAIDĖJAI
                        if (val == target_num) {
                            char win_msg[100];
                            sprintf(win_msg, "\n!!! ATSPEJO !!! Zaidejas porte %d laimejo. Skaicius buvo: %d\n", ports[i], target_num);
                            broadcast(client_fds, win_msg);
                            fprintf(outputFile, "Zaidejams siusta: \n!!! ATSPEJO !!! Zaidejas porte %d laimejo. Skaicius buvo: %d\n", ports[i], target_num);
                            target_num = -1; // Resetinam žaidimą
                            broadcast(client_fds, "Laukiam kol vadovas nustatys nauja skaiciu...\n");
                            fprintf(outputFile, "Zaidejams siusta: Laukiam kol vadovas nustatys nauja skaiciu...\n");
                        } else {
                            // 1. Sukuriam pranešimą visiems apie atliktą spėjimą
                            char broadcast_guess[100];
                            sprintf(broadcast_guess, "\nZaidejas porte %d spejo skaiciu: %d\n", ports[i], val);
                            broadcast(client_fds, broadcast_guess);
                            fprintf(outputFile, "Visems siusta: \nZaidejas porte %d spejo skaiciu: %d\n", ports[i], val);

                            // 2. Nusiunčiam užuominą tik spėjusiam žaidėjui
                            char hint[50];
                            sprintf(hint, "Skaicius yra %s.\n", (val < target_num ? "DIDESNIS" : "MAZESNIS"));
                            send(client_fds[i], hint, strlen(hint), 0);
                            fprintf(outputFile, "Zaidejui %d siusta: \nZaidejas porte %d spejo skaiciu: %d\n", client_fds[i], ports[i], val);
                            
                            // 3. Eilės perdavimas (lieka toks pat)
                            do {
                                current_turn = (current_turn % 3) + 1;
                            } while (client_fds[current_turn] == -1);

                            send(client_fds[current_turn], "Tavo eile speti: ", 17, 0);
                            fprintf(outputFile, "Zaidejui %d siusta: Tavo eile speti: ", client_fds[current_turn]);
                        }
                    } else if (target_num != -1 && i != current_turn) {
                        send(client_fds[i], "Dar ne tavo eile!\n", 18, 0);
                        fprintf(outputFile, "Zaidejui %d siusta: Dar ne tavo eile!\n", client_fds[i]);
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    close(client_fds[i]);
                    client_fds[i] = -1;
                }
            }
        }
        usleep(10000); 
        fclose(outputFile);
    }
    return 0;
}