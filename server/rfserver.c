#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 2024
#define BUFFER_SIZE 1024

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];

    // 1. get command, remote path, file size
    read(client_sock, buffer, sizeof(buffer));
    char *cmd = strtok(buffer, "\n");
    if (strcmp(cmd, "WRITE") != 0) {
        fprintf(stderr, "Unsupported command: %s\n", cmd);
        close(client_sock);
        return;
    }
    char *remote_path = strtok(NULL, "\n");
    char *file_size_str = strtok(NULL, "\n");
    long file_size = atol(file_size_str);

    // 2. write file to disk
    FILE *fp = fopen(remote_path, "wb");
    if (!fp) {
        perror("Cannot create file on server");
        close(client_sock);
        return;
    }

    long received = 0;
    while (received < file_size) {
        ssize_t n = read(client_sock, buffer, BUFFER_SIZE);
        fwrite(buffer, 1, n, fp);
        received += n;
    }

    fclose(fp);
    printf("Received and saved file to %s\n", remote_path);
    close(client_sock);
}

int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);
        handle_client(client_sock);
    }

    close(server_sock);
    return 0;
}