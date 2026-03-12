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

int main(void) {
    int ports[4] = {20000, 20001, 20002, 20003};
    int server_fds[4];
    int client_fds[4] = {-1, -1, -1, -1};
    int target_num = -1;
    int current_turn = 1;

    for (int i = 0; i < 4; i++) {
        // 1. Sukuriam IPv6 lizdą
        server_fds[i] = socket(AF_INET6, SOCK_STREAM, 0);
        if (server_fds[i] < 0) {
            perror("socket");
            exit(1);
        }

        // 2. Leidžiam greitai pakartotinai naudoti portą
        int opt = 1;
        setsockopt(server_fds[i], SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

        // 3. IŠJUNGIAM IPV6_V6ONLY (kad veiktų ir IPv4)
        int v6only = 0;
        if (setsockopt(server_fds[i], IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only)) < 0) {
            perror("setsockopt v6only");
        }

        // 4. Bind'inam prie visų sąsajų
        struct sockaddr_in6 addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin6_family = AF_INET6;
        addr.sin6_port = htons(ports[i]);
        addr.sin6_addr = in6addr_any; // Klausosi [::] (visų IPv6 ir IPv4 adresų)

        if (bind(server_fds[i], (struct sockaddr *)&addr, sizeof(addr)) < 0) {
            printf("Klaida bind'inant portą %d: %s\n", ports[i], strerror(errno));
            exit(1);
        }

        listen(server_fds[i], 5);
        set_nonblocking(server_fds[i]);
        printf("Serveris %d (IPv4+IPv6) laukia porte %d...\n", i, ports[i]);
    }

    while (1) {
        for (int i = 0; i < 4; i++) {
            // Bandome priimti naują ryšį
            int new_fd = accept(server_fds[i], NULL, NULL);
            if (new_fd != -1) {
                set_nonblocking(new_fd);
                if (client_fds[i] != -1) close(client_fds[i]);
                client_fds[i] = new_fd;
                printf("Prisijungta prie porto %d\n", ports[i]);
                send(new_fd, "Sveiki! Iveskite skaiciu.\n", 26, 0);
            }

            // Tikriname duomenis
            if (client_fds[i] != -1) {
                char buf[BUFSIZE];
                int n = recv(client_fds[i], buf, BUFSIZE - 1, 0);

                if (n > 0) {
                    buf[n] = '\0';
                    int val = atoi(buf);
                    
                    if (i == 0) { // Vadovas
                        target_num = val;
                        printf("VADOVAS (20000) nustate: %d\n", target_num);
                        for(int j=1; j<4; j++) if(client_fds[j] != -1) send(client_fds[j], "Pradedam!\n", 10, 0);
                    } else if (target_num != -1 && i == current_turn) { // Žaidėjai
                        if (val == target_num) {
                            send(client_fds[i], "ATSPĖJAI!\n", 11, 0);
                            target_num = -1;
                        } else {
                            send(client_fds[i], (val < target_num ? "Daugiau\n" : "Mažiau\n"), 9, 0);
                            current_turn = (current_turn % 3) + 1;
                        }
                    }
                } else if (n == 0 || (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK)) {
                    // Klientas atsijungė arba įvyko klaida
                    close(client_fds[i]);
                    client_fds[i] = -1;
                }
            }
        }
        usleep(10000); 
    }
    return 0;
}