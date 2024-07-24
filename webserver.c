#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <pthread.h>

#define HTML_FILE "index.html"
#define PORT 8080
#define BUF_SIZE 1024

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

    FILE *html_file = fopen(HTML_FILE, "r");
    if (!html_file) {
        perror("html file error");
        close(client_socket);
        free(client_info);
        return NULL;
    }

    // we get file size by trolling the pointers
    fseek(html_file, 0, SEEK_END);
    long file_size = ftell(html_file);
    rewind(html_file);

    // malloc my beloved
    char *html_content = malloc(file_size + 1);
    if (!html_content) {
        perror("malloc error");
        fclose(html_file);
        close(client_socket);
        return NULL;
    }

    fread(html_content, 1, file_size, html_file);
    html_content[file_size] = '\0';
    fclose(html_file);

    char response_header[BUF_SIZE];
    snprintf(response_header, sizeof(response_header),
             "HTTP/1.1 200 OK\r\n"
             "Content-Type: text/html\r\n"
             "Content-Length: %ld\r\n"
             "\r\n",
             file_size);

    // writing to this sends the response header
    write(client_socket, response_header, strlen(response_header));
    
    // writing to this sends the HTML content
    write(client_socket, html_content, file_size);

    // Free the allocated memory
    free(html_content);

    // Close the connection
    close(client_socket);
    free(client_info);
    return NULL;
}
