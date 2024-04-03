#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <string.h>
#include <unistd.h>

#define BUFFER_SIZE 256

void usage(char *prg_name) {
  fprintf(stderr, "Usage: %s <port> <host_ip> <client_name>\n", prg_name);
  exit(EXIT_FAILURE);
}

int connect_to_host(const char *ip, const int port) {
  int host_fd = socket(AF_INET, SOCK_STREAM, 0);

  struct sockaddr_in host_addr = {
    .sin_family = AF_INET,
    .sin_port = htons((uint16_t) port)
  };
  inet_pton(AF_INET, ip, &(host_addr.sin_addr));

  if (connect(host_fd, (struct sockaddr *)&host_addr, sizeof(host_addr)) == -1)
  {
    return -1;
  }

  return host_fd;
}

int main(int argc, char **argv) {
  if (argc != 4) usage(argv[0]);

  char buffer[BUFFER_SIZE];
  const int  port         = atoi(argv[1]);
  const char *host_ip     = argv[2];
  const char *client_name = argv[3];

  int host_fd = connect_to_host(host_ip, port);
  if (host_fd == -1) {
    fprintf(stderr, "%s() :: failed to acquire host file descriptor.\n",
            __func__);
    return 1;
  }

  // Welcome MSG
  printf("Connected to the server as %s.\n", client_name);
  ssize_t bytes_received = recv(host_fd, buffer, BUFFER_SIZE - 1, 0);
  buffer[bytes_received] = '\0';
  printf("HOST :: %s\n", buffer);

  while (1) {
    printf("$ ");
    if (fgets(buffer, BUFFER_SIZE, stdin) == NULL) {
      fprintf(stderr, "%s() :: an error occurred or EOF was reached.\n",
              __func__);
      break;
    }

    // remove newline character from input if present
    buffer[strcspn(buffer, "\n")] = 0;

    if (strcmp(buffer, "exit") == 0) break;

    // Send message to the server
    if (send(host_fd, buffer, strlen(buffer), 0) == -1) {
      fprintf(stderr, "%s() :: send failed", __func__);
      break;
    }

    // Receive response from the server
    ssize_t bytes_received = recv(host_fd, buffer, BUFFER_SIZE - 1, 0);
    if (bytes_received == -1) {
      fprintf(stderr, "%s() :: recv failed", __func__);
      break;
    } else if (bytes_received == 0) {
      printf("%s() :: erver closed the connection.\n", __func__);
      break;
    }
    buffer[bytes_received] = '\0'; // Null-terminate the received data
    printf("--> %s\n", buffer);
  }

  close(host_fd);
  return 0;
}
