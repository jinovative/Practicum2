#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/stat.h>

#define SERVER_IP "127.0.0.1"
#define SERVER_PORT 2024
#define BUFFER_SIZE 1024

void handle_rm(int client_sock, const char *path);

void write_command(const char *local_path, const char *remote_path) {
    // 1. open file
    FILE *fp = fopen(local_path, "rb");
    if (!fp) {
        perror("Cannot open local file");
        exit(1);
    }

    // 2. calculate file size in local
    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    rewind(fp);

    // 3. allocate memory for file data
    char *file_data = malloc(fsize);
    fread(file_data, 1, fsize, fp);
    fclose(fp);

    // 4. connect to server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // 5. send file data protocol
    dprintf(sock, "WRITE\n%s\n%ld\n", remote_path, fsize);
    write(sock, file_data, fsize);

    // 6. clean up
    free(file_data);
    close(sock);
    printf("File sent successfully.\n");
}

void get_command(const char *remote_path, const char *local_path) {
    // 1. connect to server
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    // 2. request file
    dprintf(sock, "GET\n%s\n", remote_path);

    // 3. receive file size
    char header[100];
    read(sock, header, sizeof(header));
    long file_size = atol(header);  // 크기 파싱

    // 4. save file
    FILE *fp = fopen(local_path, "wb");
    if (!fp) {
        perror("Cannot open file to write");
        close(sock);
        return;
    }

    char buffer[BUFFER_SIZE];
    long received = 0;
    while (received < file_size) {
        ssize_t n = read(sock, buffer, BUFFER_SIZE);
        fwrite(buffer, 1, n, fp);
        received += n;
    }

    fclose(fp);
    close(sock);
    printf("File received and saved to %s\n", local_path);
}

void rm_command(const char *remote_path) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in server_addr;
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(SERVER_PORT);
    server_addr.sin_addr.s_addr = inet_addr(SERVER_IP);
    connect(sock, (struct sockaddr *)&server_addr, sizeof(server_addr));

    dprintf(sock, "RM\n%s\n", remote_path);

    char buffer[BUFFER_SIZE];
    read(sock, buffer, sizeof(buffer));
    printf("Server response: %s\n", buffer);

    close(sock);
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: %s COMMAND args...\n", argv[0]);
        return 1;
    }

    if (strcmp(argv[1], "WRITE") == 0 && argc == 4) {
        write_command(argv[2], argv[3]);
    } else if (strcmp(argv[1], "GET") == 0 && argc == 4) {
        get_command(argv[2], argv[3]);
    } 
    else if (strcmp(argv[1], "RM") == 0 && argc == 3) {
        rm_command(argv[2]);
    }
    else {
        fprintf(stderr, "Invalid usage.\n");
        return 1;
    }

    return 0;
}