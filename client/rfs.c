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

int main(int argc, char *argv[]) {
    if (argc != 4 || strcmp(argv[1], "WRITE") != 0) {
        fprintf(stderr, "Usage: %s WRITE local_file remote_file\n", argv[0]);
        return 1;
    }
    write_command(argv[2], argv[3]);
    return 0;
}