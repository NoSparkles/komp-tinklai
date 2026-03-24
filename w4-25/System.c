#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <netinet/in.h>

#define MAX_CLIENTS 30
#define BUFFER_SIZE 2048
#define QUEUE_DELAY 5

typedef struct Msg {
    char from[50], to[50], content[1024];
    int from_sid; 
    time_t timestamp;
    struct Msg *next;
} Msg;

typedef struct {
    int socket;
    char vardas[50];
    int nustatytas;
} Klientas;

// --- Admin Hub Logic ---
void run_admin(int port) {
    int node_sockets[3] = {-1, -1, -1}; 
    Klientas human_admin = {-1, "", 0};
    Msg *queue = NULL;
    char banned_users[100][50];
    int banned_count = 0;

    // Use AF_INET6 for dual-stack support
    int server_fd = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin6_family = AF_INET6;
    addr.sin6_port = htons(port);
    addr.sin6_addr = in6addr_any; // Accept any address (IPv4 or IPv6)

    int opt = 1; 
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    // Ensure IPv4 users can still connect to this IPv6 socket
    int v6only = 0;
    setsockopt(server_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));

    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("[SYSTEM] Admin Hub (Dual-Stack) started on port %d\n", port);

    while(1) {
        fd_set fds; FD_ZERO(&fds);
        FD_SET(server_fd, &fds);
        int max_sd = server_fd;

        if(node_sockets[1] > 0) { FD_SET(node_sockets[1], &fds); if(node_sockets[1] > max_sd) max_sd = node_sockets[1]; }
        if(node_sockets[2] > 0) { FD_SET(node_sockets[2], &fds); if(node_sockets[2] > max_sd) max_sd = node_sockets[2]; }
        if(human_admin.socket > 0) { FD_SET(human_admin.socket, &fds); if(human_admin.socket > max_sd) max_sd = human_admin.socket; }

        struct timeval tv = {1, 0}; 
        select(max_sd + 1, &fds, NULL, NULL, &tv);

        if (FD_ISSET(server_fd, &fds)) {
            int ns = accept(server_fd, NULL, NULL);
            send(ns, "ATSIUSKVARDA\n", 13, 0);
            char ident[50];
            int r = recv(ns, ident, 49, 0);
            if (r > 0) {
                ident[r] = '\0'; strtok(ident, "\r\n ");
                if (strcmp(ident, "S1") == 0) node_sockets[1] = ns;
                else if (strcmp(ident, "S2") == 0) node_sockets[2] = ns;
                else {
                    human_admin.socket = ns; strncpy(human_admin.vardas, ident, 49);
                    human_admin.nustatytas = 1; send(ns, "VARDASOK\n", 9, 0);
                }
            }
        }

        if(human_admin.socket > 0 && FD_ISSET(human_admin.socket, &fds)) {
            char abuf[BUFFER_SIZE];
            int av = recv(human_admin.socket, abuf, BUFFER_SIZE-1, 0);
            if(av <= 0) { close(human_admin.socket); human_admin.socket = -1; }
            else {
                abuf[av] = '\0';
                if(strncmp(abuf, "#stop", 5) == 0) {
                    char target[50]; sscanf(abuf, "#stop %s", target);
                    strcpy(banned_users[banned_count++], target);
                    char ban_cmd[100];
                    snprintf(ban_cmd, 100, "BAN|SYSTEM|%s", target);
                    if(node_sockets[1] > 0) send(node_sockets[1], ban_cmd, strlen(ban_cmd), 0);
                    if(node_sockets[2] > 0) send(node_sockets[2], ban_cmd, strlen(ban_cmd), 0);

                    Msg **curr = &queue;
                    while (*curr) {
                        if (strcmp((*curr)->from, target) == 0) {
                            Msg *temp = *curr; *curr = (*curr)->next; free(temp);
                        } else curr = &((*curr)->next);
                    }
                    send(human_admin.socket, "PRANESIMAS SYSTEM: User banned.\n", 33, 0);
                }
            }
        }

        for(int i = 1; i <= 2; i++) {
            if(node_sockets[i] > 0 && FD_ISSET(node_sockets[i], &fds)) {
                char buf[BUFFER_SIZE];
                int val = recv(node_sockets[i], buf, sizeof(buf)-1, 0);
                if(val <= 0) { close(node_sockets[i]); node_sockets[i] = -1; continue; }
                buf[val] = '\0';

                char f[50], t[50], c[1024];
                if(sscanf(buf, "%[^|]|%[^|]|%[^\n]", f, t, c) == 3) {
                    if(human_admin.socket > 0) {
                        char logmsg[BUFFER_SIZE];
                        snprintf(logmsg, BUFFER_SIZE, "PRANESIMAS %s (to %s): %s\n", f, t, c);
                        send(human_admin.socket, logmsg, strlen(logmsg), 0);
                    }
                    Msg *n = malloc(sizeof(Msg));
                    strcpy(n->from, f); strcpy(n->to, t); strcpy(n->content, c);
                    n->from_sid = i; n->timestamp = time(NULL); n->next = queue;
                    queue = n;
                }
            }
        }

        time_t now = time(NULL);
        Msg **m = &queue;
        while (*m) {
            if (difftime(now, (*m)->timestamp) >= QUEUE_DELAY) {
                char out[BUFFER_SIZE];
                snprintf(out, sizeof(out), "%s|%s|%s", (*m)->to, (*m)->from, (*m)->content);
                int dest = (strcmp((*m)->to, "@all") == 0) ? (*m)->from_sid : (((*m)->from_sid == 1) ? 2 : 1);
                if(node_sockets[dest] > 0) send(node_sockets[dest], out, strlen(out), 0);
                Msg *del = *m; *m = (*m)->next; free(del);
            } else m = &((*m)->next);
        }
    }
}

// --- Node Logic ---
void run_node(int my_port, int admin_port, char *id) {
    sleep(1);
    // Connect to Admin using IPv6 loopback
    int admin_fd = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 a_addr;
    memset(&a_addr, 0, sizeof(a_addr));
    a_addr.sin6_family = AF_INET6;
    a_addr.sin6_port = htons(admin_port);
    inet_pton(AF_INET6, "::1", &a_addr.sin6_addr);
    connect(admin_fd, (struct sockaddr *)&a_addr, sizeof(a_addr));
    
    char name_req[50]; recv(admin_fd, name_req, 49, 0);
    send(admin_fd, id, strlen(id), 0);

    // Node Server Socket (IPv6 Dual-Stack)
    int s_fd = socket(AF_INET6, SOCK_STREAM, 0);
    struct sockaddr_in6 m_addr;
    memset(&m_addr, 0, sizeof(m_addr));
    m_addr.sin6_family = AF_INET6;
    m_addr.sin6_port = htons(my_port);
    m_addr.sin6_addr = in6addr_any;

    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    int v6only = 0; setsockopt(s_fd, IPPROTO_IPV6, IPV6_V6ONLY, &v6only, sizeof(v6only));
    
    bind(s_fd, (struct sockaddr *)&m_addr, sizeof(m_addr));
    listen(s_fd, 5);

    Klientas clients[MAX_CLIENTS];
    for(int i=0; i<MAX_CLIENTS; i++) clients[i].socket = -1;

    while(1) {
        fd_set fds; FD_ZERO(&fds);
        FD_SET(s_fd, &fds); FD_SET(admin_fd, &fds);
        int max = (s_fd > admin_fd) ? s_fd : admin_fd;
        for(int i=0; i<MAX_CLIENTS; i++) if(clients[i].socket > 0) {
            FD_SET(clients[i].socket, &fds); if(clients[i].socket > max) max = clients[i].socket;
        }
        select(max + 1, &fds, NULL, NULL, NULL);

        if(FD_ISSET(admin_fd, &fds)) {
            char b[BUFFER_SIZE], target[50], sender[50], content[1024];
            int v = recv(admin_fd, b, sizeof(b)-1, 0);
            if(v > 0) {
                b[v] = '\0';
                if(sscanf(b, "%[^|]|%[^|]|%[^\n]", target, sender, content) == 3) {
                    if(strcmp(target, "BAN") == 0) {
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if(clients[i].socket > 0 && strcmp(clients[i].vardas, content) == 0) {
                                send(clients[i].socket, "PRANESIMAS SYSTEM: You are banned.\n", 35, 0);
                                close(clients[i].socket); clients[i].socket = -1;
                            }
                        }
                    } else {
                        char final_msg[BUFFER_SIZE];
                        snprintf(final_msg, BUFFER_SIZE, "PRANESIMAS %s: %s\n", sender, content);
                        for(int i=0; i<MAX_CLIENTS; i++) {
                            if(clients[i].socket > 0 && clients[i].nustatytas) {
                                if(strcmp(target, "@all") == 0 || strcmp(target, clients[i].vardas) == 0)
                                    send(clients[i].socket, final_msg, strlen(final_msg), 0);
                            }
                        }
                    }
                }
            }
        }

        if(FD_ISSET(s_fd, &fds)) {
            int ns = accept(s_fd, NULL, NULL);
            for(int i=0; i<MAX_CLIENTS; i++) if(clients[i].socket == -1) {
                clients[i].socket = ns; clients[i].nustatytas = 0;
                send(ns, "ATSIUSKVARDA\n", 13, 0); break;
            }
        }

        for(int i=0; i<MAX_CLIENTS; i++) {
            int sd = clients[i].socket;
            if(sd > 0 && FD_ISSET(sd, &fds)) {
                char b[BUFFER_SIZE]; int v = read(sd, b, BUFFER_SIZE-1);
                if(v <= 0) { close(sd); clients[i].socket = -1; }
                else {
                    b[v] = '\0'; strtok(b, "\r\n");
                    if(!clients[i].nustatytas) { 
                        strncpy(clients[i].vardas, b, 49); clients[i].nustatytas = 1; 
                        send(sd, "VARDASOK\n", 9, 0); 
                    } else {
                        char pkg[BUFFER_SIZE];
                        if(b[0] == '@') {
                            char t[50], m[1024];
                            if(sscanf(b, "@%s %[^\n]", t, m) == 2)
                                snprintf(pkg, sizeof(pkg), "%s|%s|%s", clients[i].vardas, t, m);
                        } else snprintf(pkg, sizeof(pkg), "%s|@all|%s", clients[i].vardas, b);
                        send(admin_fd, pkg, strlen(pkg), 0);
                    }
                }
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc < 2) { printf("Usage: %s <Port>\n", argv[0]); return 1; }
    int p = atoi(argv[1]);
    if (fork() == 0) run_node(p+1, p, "S1");
    else if (fork() == 0) run_node(p+2, p, "S2");
    else run_admin(p);
    return 0;
}