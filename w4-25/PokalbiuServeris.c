#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <sys/wait.h> // Reikalinga procesų valdymui

#define BUFFER_SIZE 1024

void handle_client(int client_socket) {
    char buffer[BUFFER_SIZE];
    char vardas[50];
    int valread;

    // 1. Paprašome vardo
    send(client_socket, "ATSIUSKVARDA\n", 13, 0);
    valread = read(client_socket, buffer, BUFFER_SIZE - 1);
    if (valread <= 0) {
        close(client_socket);
        exit(0);
    }
    buffer[valread] = '\0';
    strtok(buffer, "\r\n");
    strncpy(vardas, buffer, 49);
    send(client_socket, "VARDASOK\n", 9, 0);

    // 2. Kliento bendravimo ciklas
    while ((valread = read(client_socket, buffer, BUFFER_SIZE - 1)) > 0) {
        buffer[valread] = '\0';
        strtok(buffer, "\r\n");
        
        printf("Klientas %s sako: %s\n", vardas, buffer);
        
        // Svarbu: šiame paprastame modelyje vaikas mato tik SAVO klientą.
        // Norint nusiųsti kitiems, reikėtų sudėtingesnių IPC mechanizmų.
        char msg[BUFFER_SIZE + 60];
        sprintf(msg, "TU SAKEI: %s\n", buffer);
        send(client_socket, msg, strlen(msg), 0);
    }

    printf("Klientas %s atsijungė.\n", vardas);
    close(client_socket);
    exit(0); // Vaikinis procesas baigia darbą
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        printf("Naudojimas: %s <portas>\n", argv[0]);
        return 1;
    }

    int portas = atoi(argv[1]);
    int server_fd, naujas_soketas;
    struct sockaddr_in adresas;

    server_fd = socket(AF_INET, SOCK_STREAM, 0);
    int opt = 1;
    setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    adresas.sin_family = AF_INET;
    adresas.sin_addr.s_addr = INADDR_ANY;
    adresas.sin_port = htons(portas);

    bind(server_fd, (struct sockaddr *)&adresas, sizeof(adresas));
    listen(server_fd, 5);

    printf("Fork-serveris veikia porte %d...\n", portas);

    while (1) {
        int addrlen = sizeof(adresas);
        naujas_soketas = accept(server_fd, (struct sockaddr *)&adresas, (socklen_t *)&addrlen);
        
        if (naujas_soketas < 0) continue;

        // Sukuriame naują procesą
        pid_t pid = fork();

        if (pid == 0) {
            // Tai yra VAIKINIS procesas
            close(server_fd); // Vaikui nereikia klausytis naujų jungčių
            handle_client(naujas_soketas);
        } else if (pid > 0) {
            // Tai yra TĖVINIS procesas
            close(naujas_soketas); // Tėvui šio konkretaus soketo nebereikia
            
            // Sutvarkome baigtus procesus (zombius)
            waitpid(-1, NULL, WNOHANG);
        } else {
            perror("Fork klaida");
        }
    }

    return 0;
}