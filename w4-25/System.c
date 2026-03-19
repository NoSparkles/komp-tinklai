#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/select.h>
#include <sys/wait.h>

#define MAX_CLIENTS 30
#define BUFFER_SIZE 2048
#define QUEUE_DELAY 10

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
    char banned[100][50];
    int banned_count = 0;

    int server_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr = {AF_INET, htons(port), INADDR_ANY};
    int opt = 1; setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
    bind(server_fd, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_fd, 5);

    printf("[SYSTEM] Admin Hub started on port %d\n", port);

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
            if(av <= 0) human_admin.socket = -1;
            else {
                abuf[av] = '\0';
                if(strncmp(abuf, "#stop", 5) == 0) {
                    char target[50]; sscanf(abuf, "#stop %s", target);
                    strcpy(banned[banned_count++], target);
                    Msg **curr = &queue;
                    while (*curr) {
                        if (strcmp((*curr)->from, target) == 0) {
                            Msg *temp = *curr; *curr = (*curr)->next; free(temp);
                        } else curr = &((*curr)->next);
                    }
                    send(human_admin.socket, "DONE: User banned, queue purged.\n", 33, 0);
                }
            }
        }

        for(int i = 1; i <= 2; i++) {
            if(node_sockets[i] > 0 && FD_ISSET(node_sockets[i], &fds)) {
                char buf[BUFFER_SIZE];
                int val = recv(node_sockets[i], buf, sizeof(buf)-1, 0);
                if(val <= 0) { node_sockets[i] = -1; continue; }
                buf[val] = '\0';

                char f[50], t[50], c[1024];
                if(sscanf(buf, "%[^|]|%[^|]|%[^\n]", f, t, c) == 3) {
                    // IMMEDIATELY notify admin
                    if(human_admin.socket > 0) {
                        char logmsg[BUFFER_SIZE];
                        snprintf(logmsg, BUFFER_SIZE, "[INCOMING] %s to %s: %s\n", f, t, c);
                        send(human_admin.socket, logmsg, strlen(logmsg), 0);
                    }
                    // Add to queue
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
                // Protocol: TARGET|SENDER|CONTENT
                snprintf(out, sizeof(out), "%s|%s|%s", (*m)->to, (*m)->from, (*m)->content);
                if (strcmp((*m)->to, "@all") == 0) {
                    send(node_sockets[(*m)->from_sid], out, strlen(out), 0);
                } else {
                    int tsid = ((*m)->from_sid == 1) ? 2 : 1;
                    if(node_sockets[tsid] > 0) send(node_sockets[tsid], out, strlen(out), 0);
                }
                Msg *del = *m; *m = (*m)->next; free(del);
            } else m = &((*m)->next);
        }
    }
}

// --- Node Logic (S1/S2) ---
void run_node(int my_port, int admin_port, char *id) {
    sleep(1);
    int admin_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in a_addr = {AF_INET, htons(admin_port), INADDR_ANY};
    connect(admin_fd, (struct sockaddr *)&a_addr, sizeof(a_addr));
    char name_req[50]; recv(admin_fd, name_req, 49, 0);
    send(admin_fd, id, strlen(id), 0);

    int s_fd = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in m_addr = {AF_INET, htons(my_port), INADDR_ANY};
    int opt = 1; setsockopt(s_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
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
                    char final_msg[BUFFER_SIZE];
                    snprintf(final_msg, BUFFER_SIZE, "PRANESIMAS %s: %s\n", sender, content);
                    for(int i=0; i<MAX_CLIENTS; i++) {
                        if(clients[i].socket > 0 && clients[i].nustatytas) {
                            // Logic: If target is @all, send to everyone. If specific name, only send to that person.
                            if(strcmp(target, "@all") == 0 || strcmp(target, clients[i].vardas) == 0) {
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