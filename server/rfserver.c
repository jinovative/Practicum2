#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>

#define PORT 2024
#define BUFFER_SIZE 1024

void handle_get(int client_sock, const char *path);
void handle_rm(int client_sock, const char *path);

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    read(client_sock, buffer, sizeof(buffer));

    char *cmd = strtok(buffer, "\n");
    char *arg1 = strtok(NULL, "\n");
    char *arg2 = strtok(NULL, "\n");  // only for WRITE

    if (strcmp(cmd, "WRITE") == 0) {
        // handle write command
        long file_size = atol(arg2);
        FILE *fp = fopen(arg1, "wb");
        long received = 0;
        while (received < file_size) {
            ssize_t n = read(client_sock, buffer, BUFFER_SIZE);
            fwrite(buffer, 1, n, fp);
            received += n;
        }
        fclose(fp);
    }
    else if (strcmp(cmd, "GET") == 0) {
        handle_get(client_sock, arg1);
    }

    else if (strcmp(cmd, "RM") == 0) {
        handle_rm(client_sock, arg1);
    }

    close(client_sock);
}

void handle_get(int client_sock, const char *path) {
    FILE *fp = fopen(path, "rb");
    if (!fp) {
        perror("File not found on server");
        dprintf(client_sock, "0\n"); // not found
        return;
    }

    // calculate file size
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);
    dprintf(client_sock, "%ld\n", fsize);

    // send file data
    char buffer[BUFFER_SIZE];
    long sent = 0;
    while (sent < fsize) {
        size_t n = fread(buffer, 1, BUFFER_SIZE, fp);
        send(client_sock, buffer, n, 0);
        sent += n;
    }

    fclose(fp);
}

#include <errno.h>

void handle_rm(int client_sock, const char *path) {
    int result = remove(path);
    if (result == 0) {
        dprintf(client_sock, "OK\n");
        printf("Deleted file: %s\n", path);
    } else {
        dprintf(client_sock, "FAIL\n");
        perror("Delete failed");
    }
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