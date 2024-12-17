#include <arpa/inet.h>
#include <fcntl.h>
#include <getopt.h>
#include <netinet/in.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define DEFAULT_PORT 80
#define BUFFER_SIZE 1024
#define STATIC_DIR "./static"
#define MAX_REQUESTS 1000

typedef struct {
  int client_socket;
  struct sockaddr_in client_address;
} client_info_t;

int request_count = 0;
int total_received_bytes = 0;
int total_sent_bytes = 0;
pthread_mutex_t stats_mutex = PTHREAD_MUTEX_INITIALIZER;

void handle_client_request(client_info_t *client_info) {
  char buffer[BUFFER_SIZE];
  int bytes_received =
      recv(client_info->client_socket, buffer, BUFFER_SIZE - 1, 0);
  if (bytes_received < 0) {
    perror("recv");
    close(client_info->client_socket);
    free(client_info);
    return;
  }
  buffer[bytes_received] = '\0';

  pthread_mutex_lock(&stats_mutex);
  request_count++;
  total_received_bytes += bytes_received;
  pthread_mutex_unlock(&stats_mutex);

  // Parse the request
  char method[16], path[256], protocol[16];
  sscanf(buffer, "%s %s %s", method, path, protocol);

  if (strcmp(method, "GET") == 0) {
    if (strncmp(path, "/static/", 8) == 0) {
      char file_path[512];
      snprintf(file_path, sizeof(file_path), "%s%s", STATIC_DIR, path + 7);
      int file_fd = open(file_path, O_RDONLY);
      if (file_fd < 0) {
        char *not_found_response = "HTTP/1.1 404 Not Found\r\n\r\n";
        send(client_info->client_socket, not_found_response,
             strlen(not_found_response), 0);
      } else {
        struct stat file_stat;
        fstat(file_fd, &file_stat);
        char header[BUFFER_SIZE];
        snprintf(header, sizeof(header),
                 "HTTP/1.1 200 OK\r\nContent-Length: %ld\r\n\r\n",
                 file_stat.st_size);
        send(client_info->client_socket, header, strlen(header), 0);
        int bytes_sent;
        while ((bytes_sent = read(file_fd, buffer, BUFFER_SIZE)) > 0) {
          send(client_info->client_socket, buffer, bytes_sent, 0);
          pthread_mutex_lock(&stats_mutex);
          total_sent_bytes += bytes_sent;
          pthread_mutex_unlock(&stats_mutex);
        }
        close(file_fd);
      }
    } else if (strcmp(path, "/stats") == 0) {
      // Handle /stats endpoint
      char stats_response[BUFFER_SIZE];
      pthread_mutex_lock(&stats_mutex);
      snprintf(stats_response, sizeof(stats_response),
               "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
               "<html><body>"
               "<h1>Server Stats</h1>"
               "<p>Requests received: %d</p>"
               "<p>Total received bytes: %d</p>"
               "<p>Total sent bytes: %d</p>"
               "</body></html>",
               request_count, total_received_bytes, total_sent_bytes);
      pthread_mutex_unlock(&stats_mutex);
      send(client_info->client_socket, stats_response, strlen(stats_response),
           0);
    } else if (strncmp(path, "/calc?", 6) == 0) {
      // Handle /calc endpoint
      int aa = 0, bb = 0;
      sscanf(path, "/calc?aa=%d&bb=%d", &aa, &bb);
      char calc_response[BUFFER_SIZE];
      snprintf(calc_response, sizeof(calc_response),
               "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n"
               "<html><body>"
               "<h1>Calculation Result</h1>"
               "<p>%d + %d = %d</p>"
               "</body></html>",
               aa, bb, aa + bb);
      send(client_info->client_socket, calc_response, strlen(calc_response), 0);
    } else {
      // Handle unknown endpoint
      char *not_found_response = "HTTP/1.1 404 Not Found\r\n\r\n";
      send(client_info->client_socket, not_found_response,
           strlen(not_found_response), 0);
    }
  }

  close(client_info->client_socket);
  free(client_info);
}

void *client_thread(void *arg) {
  client_info_t *client_info = (client_info_t *)arg;
  handle_client_request(client_info);
  return NULL;
}

void start_server(int port) {
  int server_socket = socket(AF_INET, SOCK_STREAM, 0);
  if (server_socket < 0) {
    perror("socket failed");
    exit(EXIT_FAILURE);
  }

  struct sockaddr_in server_address;
  server_address.sin_family = AF_INET;
  server_address.sin_addr.s_addr = INADDR_ANY;
  server_address.sin_port = htons(port);

  if (bind(server_socket, (struct sockaddr *)&server_address,
           sizeof(server_address)) < 0) {
    perror("bind failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  if (listen(server_socket, MAX_REQUESTS) < 0) {
    perror("listen failed");
    close(server_socket);
    exit(EXIT_FAILURE);
  }

  printf("Server listening on port %d\n", port);

  while (1) {
    client_info_t *client_info = malloc(sizeof(client_info_t));
    socklen_t client_len = sizeof(client_info->client_address);
    client_info->client_socket =
        accept(server_socket, (struct sockaddr *)&client_info->client_address,
               &client_len);
    if (client_info->client_socket < 0) {
      perror("accept failed");
      free(client_info);
      continue;
    }

    pthread_t thread_id;
    pthread_create(&thread_id, NULL, client_thread, client_info);
    pthread_detach(thread_id);
  }

  close(server_socket);
}

int main(int argc, char *argv[]) {
  int port = DEFAULT_PORT;

  int opt;
  while ((opt = getopt(argc, argv, "p:")) != -1) {
    switch (opt) {
    case 'p':
      port = atoi(optarg);
      break;
    default:
      fprintf(stderr, "Usage: %s [-p port]\n", argv[0]);
      exit(EXIT_FAILURE);
    }
  }

  start_server(port);

  return 0;
}