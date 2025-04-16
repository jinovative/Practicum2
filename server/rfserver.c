#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <unistd.h>
#include <pthread.h>    // Added for Option 4a (Multi-Client)
#include <sys/stat.h>   // Added for Option 4b (Permissions)
#include <dirent.h>     // Added for LS command
#include <errno.h>

#define PORT 2024
#define BUFFER_SIZE 1024 
#define MAX_PERM_FILES 100  // Added for Option 4b

// Added for Option 4a - Thread management structure
typedef struct {
    int client_sock;
} thread_args;

// Added for Option 4b - Permission types
typedef enum {
    PERM_READWRITE,  // Default permission
    PERM_READONLY
} file_permission;

// Added for Option 4b - Permission tracking
typedef struct {
    char filename[256];
    file_permission perm;
} file_perm_entry;


file_perm_entry permissions[MAX_PERM_FILES];
int perm_count = 0;
pthread_mutex_t perm_mutex = PTHREAD_MUTEX_INITIALIZER;  // For thread-safe permission access

void handle_get(int client_sock, const char *path);
void handle_rm(int client_sock, const char *path);
void handle_ls(int client_sock, const char *path); // Added for LS command

// Added for Option 4b - Check file permissions
int check_permission(const char *filename, int require_write) {
    pthread_mutex_lock(&perm_mutex);
    for (int i = 0; i < perm_count; i++) {
        if (strcmp(permissions[i].filename, filename) == 0) {
            int allowed = (require_write) ? (permissions[i].perm == PERM_READWRITE) : 1;
            pthread_mutex_unlock(&perm_mutex);
            return allowed;
        }
    }
    pthread_mutex_unlock(&perm_mutex);
    return 1; // Default to allowed if no permission entry exists
}

// Added for Option 4b - Set file permission
void set_permission(const char *filename, file_permission perm) {
    pthread_mutex_lock(&perm_mutex);
    // Update existing entry if found
    for (int i = 0; i < perm_count; i++) {
        if (strcmp(permissions[i].filename, filename) == 0) {
            permissions[i].perm = perm;
            pthread_mutex_unlock(&perm_mutex);
            return;
        }
    }
    // Add new entry if space available
    if (perm_count < MAX_PERM_FILES) {
        strncpy(permissions[perm_count].filename, filename, 255);
        permissions[perm_count].perm = perm;
        perm_count++;
    }
    pthread_mutex_unlock(&perm_mutex);
}

// Added for LS command
void handle_ls(int client_sock, const char *path) {
    struct stat st;
    if (stat(path, &st) == -1) {
        dprintf(client_sock, "ERROR: File or directory not found\n");
        return;
    }

    if (S_ISDIR(st.st_mode)) {
        // 디렉토리: 내부 항목 출력
        DIR *dir = opendir(path);
        if (!dir) {
            dprintf(client_sock, "ERROR: Could not open directory\n");
            return;
        }

        struct dirent *entry;
        while ((entry = readdir(dir)) != NULL) {
            if (strcmp(entry->d_name, ".") != 0 && strcmp(entry->d_name, "..") != 0) {
                dprintf(client_sock, "%s\n", entry->d_name);
            }
        }
        closedir(dir);
    } else {
        // 일반 파일: 메타데이터 출력
        long size = st.st_size;
        const char *perm = check_permission(path, 0) ? "READWRITE" : "READONLY";
        dprintf(client_sock, "File: %s\nSize: %ld bytes\nPermission: %s\n", path, size, perm);
    }
}

// 수정된 handle_client() 코드 

void handle_client(int client_sock) {
    // Initialize buffer and read command
    char buffer[BUFFER_SIZE] = {0};
    ssize_t bytes_read = read(client_sock, buffer, sizeof(buffer)-1); // Leave space for null terminator
    
    if (bytes_read <= 0) {
        perror("Read failed or connection closed");
        close(client_sock);
        return;
    }

    // Parse command
    char *cmd = strtok(buffer, "\n");
    char *arg1 = strtok(NULL, "\n");
    char *arg2 = strtok(NULL, "\n");  // For WRITE: file size
    char *arg3 = strtok(NULL, "\n");  // For WRITE: permission flag

    if (!cmd) {
        dprintf(client_sock, "ERROR: No command received\n");
        close(client_sock);
        return;
    }

    if (strcmp(cmd, "WRITE") == 0) {
        // Validate WRITE arguments
        if (!arg1 || !arg2) {
            dprintf(client_sock, "ERROR: WRITE requires filename and size\n");
            close(client_sock);
            return;
        }

        // Check permissions if file exists
        if (!check_permission(arg1, 1)) {
            dprintf(client_sock, "ERROR: File is read-only\n");
            close(client_sock);
            return;
        }

        // Process file transfer
        long file_size = atol(arg2);

        fprintf(stderr, "DEBUG: arg1=%s, arg2=%s, file_size=%ld, arg3=%s\n", arg1, arg2, file_size, arg3 ? arg3 : "NULL");
        if (file_size <= 0) {
            dprintf(client_sock, "ERROR: Invalid file size\n");
            close(client_sock);
            return;
        }

        FILE *fp = fopen(arg1, "wb");
        if (!fp) {
            perror("Failed to open file");
            dprintf(client_sock, "ERROR: Could not create file\n");
            close(client_sock);
            return;
        }

        long remaining = file_size;
        while (remaining > 0) {
            ssize_t n = read(client_sock, buffer, remaining < BUFFER_SIZE ? remaining : BUFFER_SIZE);
            if (n <= 0) break;
            fwrite(buffer, 1, n, fp);
            remaining -= n;
        }
        fclose(fp);

        // Set permissions
        file_permission perm = (arg3 && strcmp(arg3, "READONLY") == 0) ? PERM_READONLY : PERM_READWRITE;
        set_permission(arg1, perm);
        
        dprintf(client_sock, "OK\n");
    }
    else if (strcmp(cmd, "GET") == 0) {
        if (!arg1) {
            dprintf(client_sock, "ERROR: GET requires filename\n");
        } else {
            handle_get(client_sock, arg1);
        }
    }
    else if (strcmp(cmd, "RM") == 0) {
        if (!arg1) {
            dprintf(client_sock, "ERROR: RM requires filename\n");
        } else {
            // Check permissions and handle deletion
            if (!check_permission(arg1, 1)) {
                dprintf(client_sock, "ERROR: Cannot delete read-only file\n");
            } else {
                handle_rm(client_sock, arg1);
                // Clean up permission entry if deletion succeeded
                pthread_mutex_lock(&perm_mutex);
                for (int i = 0; i < perm_count; i++) {
                    if (strcmp(permissions[i].filename, arg1) == 0) {
                        memmove(&permissions[i], &permissions[i+1], 
                               (perm_count - i - 1) * sizeof(file_perm_entry));
                        perm_count--;
                        break;
                    }
                }
                pthread_mutex_unlock(&perm_mutex);
            }
        }
    }

    else if (strcmp(cmd, "LS") == 0) {  // This must exist
        handle_ls(client_sock, arg1);    // arg1 is the directory path
    }
    else {
        dprintf(client_sock, "ERROR: Unknown command\n");
    }

    close(client_sock);
}

/*

이전 handle_client() 코드 

void handle_client(int client_sock) {
    char buffer[BUFFER_SIZE];
    read(client_sock, buffer, sizeof(buffer));

    char *cmd = strtok(buffer, "\n");
    char *arg1 = strtok(NULL, "\n");
    char *arg2 = strtok(NULL, "\n");  // only for WRITE
    char *arg3 = strtok(NULL, "\n");  // Added for Option 4b (permission flag)

    if (strcmp(cmd, "WRITE") == 0) {

        // Check permissions if file exists (Option 4b)
        if (!check_permission(arg1, 1)) {
            dprintf(client_sock, "ERROR: File is read-only\n");
            close(client_sock);
            return;
        }


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

        // Set permission if specified (Option 4b)
        if (arg3 && strcmp(arg3, "READONLY") == 0) {
            set_permission(arg1, PERM_READONLY);
        } else {
            set_permission(arg1, PERM_READWRITE);
        }
    }
    else if (strcmp(cmd, "GET") == 0) {
        handle_get(client_sock, arg1);
    }

    else if (strcmp(cmd, "RM") == 0) {
        handle_rm(client_sock, arg1);

        // Check permissions before delete (Option 4b)
        if (!check_permission(arg1, 1)) {
            dprintf(client_sock, "ERROR: Cannot delete read-only file\n");
        } else {
            handle_rm(client_sock, arg1);
            // Remove permission entry if file was deleted
            pthread_mutex_lock(&perm_mutex);
            for (int i = 0; i < perm_count; i++) {
                if (strcmp(permissions[i].filename, arg1) == 0) {
                    // Shift remaining entries down
                    memmove(&permissions[i], &permissions[i+1], 
                           (perm_count - i - 1) * sizeof(file_perm_entry));
                    perm_count--;
                    break;
                }
            }
            pthread_mutex_unlock(&perm_mutex);
        }
    }

    close(client_sock);
} */

// Added for Option 4a - Thread wrapper function
void *client_thread(void *args) {
    thread_args *targs = (thread_args *)args;
    int client_sock = targs->client_sock;
    free(args);  // Free the allocated structure
    
    handle_client(client_sock);
    return NULL;
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
    size_t n;
    while ((n = fread(buffer, 1, BUFFER_SIZE, fp)) > 0) {
        ssize_t sent_bytes = send(client_sock, buffer, n, 0);
        if (sent_bytes <= 0) break;
    }

    fclose(fp);
}

// 수정된 handle_rm() 코드

#include <errno.h>

void handle_rm(int client_sock, const char *path) {
    // Validate path
    if (!path || strlen(path) == 0) {
        dprintf(client_sock, "ERROR: Invalid filename\n");
        return;
    }

    // Attempt deletion
    int result = remove(path);
    if (result == 0) {
        dprintf(client_sock, "OK\n");
        printf("Deleted file: %s\n", path);
    } else {
        // Detailed error reporting
        char error_msg[256];
        switch (errno) {
            case ENOENT:
                snprintf(error_msg, sizeof(error_msg), "File not found");
                break;
            case EACCES:
                snprintf(error_msg, sizeof(error_msg), "Permission denied");
                break;
            default:
                snprintf(error_msg, sizeof(error_msg), "Error %d", errno);
        }
        dprintf(client_sock, "FAIL: %s\n", error_msg);
        perror("Delete failed");
    }
}
/*

이전 handle_rm() 코드  

void handle_rm(int client_sock, const char *path) {
    int result = remove(path);
    if (result == 0) {
        dprintf(client_sock, "OK\n");
        printf("Deleted file: %s\n", path);
    } else {
        dprintf(client_sock, "FAIL\n");
        perror("Delete failed");
    }
} */



int main() {
    int server_sock = socket(AF_INET, SOCK_STREAM, 0);
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(PORT);
    addr.sin_addr.s_addr = INADDR_ANY;

     // Added for better socket handling
    int opt = 1;
    setsockopt(server_sock, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));

    bind(server_sock, (struct sockaddr *)&addr, sizeof(addr));
    listen(server_sock, 5);
    printf("Server listening on port %d...\n", PORT);

    while (1) {
        int client_sock = accept(server_sock, NULL, NULL);

        if (client_sock < 0) {
            perror("Accept failed");
            continue;
        }

        // Modified for Option 4a - Create thread for each client
        pthread_t thread_id;
        thread_args *args = malloc(sizeof(thread_args));
        args->client_sock = client_sock;

        if (pthread_create(&thread_id, NULL, client_thread, args) != 0) {
            perror("Thread creation failed");
            free(args);
            close(client_sock);
            continue;
        }

        // Detach thread so we don't need to join it later
        pthread_detach(thread_id);
        printf("New client connected in thread %lu\n", (unsigned long)thread_id);
    }
    

    close(server_sock);
    return 0;
}