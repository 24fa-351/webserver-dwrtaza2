#include <arpa/inet.h>
#include <netinet/in.h>
#include <stdio.h>
#include <sys/socket.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <stdlib.h>

#define BUFFER_SIZE 1024

void* handleConnection(void* client_fd_ptr)
{
    int client_fd = *(int*)client_fd_ptr;
    free(client_fd_ptr);

    printf("Handling connection on %d\n", client_fd);
    char buffer[BUFFER_SIZE];
    ssize_t bytes_read;

    while ((bytes_read = read(client_fd, buffer, sizeof(buffer) - 1)) > 0) {
        printf("Received: %s", buffer);
        buffer[bytes_read] = '\0';
        write(client_fd, buffer, bytes_read);
        printf("Done with connection on %d\n", client_fd);
    }

    close(client_fd);
    return NULL;
}

int main(int argc, char* argv[])
{
    if (argc != 3 || strcmp(argv[1], "-p") != 0) {
        fprintf(stderr, "Usage: %s -p <port>\n", argv[0]);
        exit(EXIT_FAILURE);
    }

    int port = atoi(argv[2]);
    int socket_fd, *client_fd_ptr;
    struct sockaddr_in server_address, client_address;
    socklen_t client_address_len = sizeof(client_address);

    socket_fd = socket(AF_INET, SOCK_STREAM, 0);
    if (socket_fd < 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    server_address.sin_family = AF_INET;
    server_address.sin_addr.s_addr = INADDR_ANY;
    server_address.sin_port = htons(port);

    if (bind(socket_fd, (struct sockaddr*)&server_address, sizeof(server_address)) < 0) {
        perror("bind failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    if (listen(socket_fd, 1) < 0) {
        perror("listen failed");
        close(socket_fd);
        exit(EXIT_FAILURE);
    }

    while (1) {
        client_fd_ptr = malloc(sizeof(int));
        if (client_fd_ptr == NULL) {
            perror("malloc failed");
            continue;
        }

        *client_fd_ptr = accept(socket_fd, (struct sockaddr*)&client_address, &client_address_len);
        if (*client_fd_ptr < 0) {
            perror("accept failed");
            free(client_fd_ptr);
            continue;
        }

        pthread_t thread_id;
        if (pthread_create(&thread_id, NULL, handleConnection, client_fd_ptr) != 0) {
            perror("pthread_create failed");
            close(*client_fd_ptr);
            free(client_fd_ptr);
        } else {
            pthread_detach(thread_id);
        }
    }

    close(socket_fd);
    return 0;
}