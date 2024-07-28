#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>
#include <dirent.h>
#include <sys/stat.h>

#define HTML_FILE "index.html"
#define PORT 8080
#define BUF_SIZE 1024
#define BASE_DIR "serve"

void *handle_client(void *arg);

typedef struct {
    int client_socket;
} client_info_t;

int main() {
    int server_socket, client_socket;
    struct sockaddr_in server_addr, client_addr;
    socklen_t client_addr_len;

    server_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (server_socket < 0) {
        perror("socket error");
        exit(EXIT_FAILURE);
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_addr.s_addr = INADDR_ANY;
    server_addr.sin_port = htons(PORT);

    if (bind(server_socket, (struct sockaddr *)&server_addr,
             sizeof(server_addr)) < 0) {
        perror("bind");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    if (listen(server_socket, 10) < 0) {
        perror("listen error");
        close(server_socket);
        exit(EXIT_FAILURE);
    }

    printf("Server listening on port %d\n", PORT);

    // Accept the connections
    while (1) {
        client_addr_len = sizeof(client_addr);
        client_socket = accept(server_socket, (struct sockaddr *)&client_addr,
                               &client_addr_len);
        if (client_socket < 0) {
            perror("accept");
            close(server_socket);
            exit(EXIT_FAILURE);
        }

        // Allocate memory for client info
        client_info_t *client_info = malloc(sizeof(client_info_t));
        if(!client_info) {
            perror("malloc error");
            close(client_socket);
            continue;
        }

        client_info->client_socket=client_socket;

        // Create thread to handle client
        pthread_t thread_id;
        if(pthread_create(&thread_id, NULL, handle_client, client_info) != 0)
        {
            perror("thread creation error");
            free(client_info);
            close(client_socket);
        }
        pthread_detach(thread_id);
    }

    // Close server socket
    close(server_socket);

    return 0;
}

void *handle_client(void *arg) {
    client_info_t *client_info = (client_info_t *)arg;
    int client_socket = client_info->client_socket;
    char buffer[BUF_SIZE];
    int bytes_read;

    // Read the request
    bytes_read = read(client_socket, buffer, BUF_SIZE - 1);
    if (bytes_read < 0) {
        perror("read");
        close(client_socket);
        free(client_info);
        return NULL;
    }

    buffer[bytes_read] = '\0';  // terminate string
    printf("Request:\n%s\n", buffer);

    // Parse request to obtain file path
    char method[16], path[256], protocol[16];
    sscanf(buffer, "%s %s %s", method, path, protocol);

    // Default to index.html if root is requested
    if (strcmp(path, "/") == 0) {
        strcpy(path, "/index.html");
    }

    // Construct full file path
    char full_path[512];
    snprintf(full_path, sizeof(full_path), "%s%s", BASE_DIR, path);

    // Check if the file exists and is a regular file
    struct stat file_stat;
    if(stat(full_path, &file_stat) < 0 || !S_ISREG(file_stat.st_mode)) {
        // File not found or is not a regular file
        const char *not_found_response = "HTTP/1.1 404 Not Found\r\n"
                                        "Content-Type: text/html\r\n"
                                        "Content-Length: 13\r\n"
                                        "\r\n"
                                        "404 Not Found";
        write(client_socket, not_found_response, strlen(not_found_response));
        close(client_socket);
        free(client_info);
        return NULL;
    }

    // Open files
    FILE *file= fopen(full_path, "r");
    if (!file) {
        perror("file open error");
        close(client_socket);
        free(client_info);
        return NULL;
    }

    // we get file size by trolling the pointers
    fseek(file, 0, SEEK_END);
    long file_size = ftell(file);
    rewind(file);

    // malloc my beloved
    char *file_content = malloc(file_size + 1);
    if (!file_content) {
        perror("malloc error");
        fclose(file);
        close(client_socket);
        return NULL;
    }

    fread(file_content, 1, file_size, file);
    file_content[file_size] = '\0';
    fclose(file);

    // Determine the content type
    const char *content_type = "text/plain";
    if (strstr(path, ".html") != NULL) {
        content_type = "text/html";
    } else if (strstr(path, ".css") != NULL) {
        content_type = "text/css";
    } else if (strstr(path, ".js") != NULL) {
        content_type = "application/javascript";
    } else if (strstr(path, ".png") != NULL) {
        content_type = "image/png";
    } else if (strstr(path, ".jpg") != NULL || strstr(path, ".jpeg") != NULL) {
        content_type = "image/jpeg";
    } else if (strstr(path, ".gif") != NULL) {
        content_type = "image/gif";
    }


    char response_header[BUF_SIZE];
    snprintf(response_header, sizeof(response_header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             content_type, file_size);

    // writing to this sends the response header
    write(client_socket, response_header, strlen(response_header));
    
    // writing to this sends the file content
    write(client_socket, file_content, file_size);

    // Free the allocated memory
    free(file_content);

    // Close the connection
    close(client_socket);
    free(client_info);
    return NULL;
}
