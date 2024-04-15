#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <unistd.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>

#define PORT     9001
#define BUF_SIZE 1024

typedef enum {
  EXIT_CLIENT_F_GETFL = 100,
  EXIT_CLIENT_F_SETFL,
  EXIT_CLIENT_SOCKET_OPEN_FAIL,
  EXIT_CLIENT_NO_HOST,
  EXIT_CLIENT_CONNECT_FAIL,
  EXIT_CLIENT_POLL_ERROR,
  EXIT_CLIENT_SERVER_CLOSE,
} client_error_t;

#define CLIENT_ERR_RANGE                \
  exit_code >= EXIT_CLIENT_F_GETFL &&   \
  exit_code <= EXIT_CLIENT_SERVER_CLOSE \

#define LOG_BRK_CLIENT_NORM(MSG) case MSG: default: fprintf(stdout, ""#MSG"\n");
#define LOG_BRK_CLIENT_FERR(ERR) case ERR: fprintf(stderr, ""#ERR"\n"); break;
#define LOG_BRK_CLIENT_PERR(ERR) case ERR: perror(""#ERR"\n"); break;

void client_exit_handler(int exit_code, void *args) {
  int sockfd = *(int *) args;
  if (CLIENT_ERR_RANGE) {
    switch (exit_code) {
    LOG_BRK_CLIENT_PERR(EXIT_CLIENT_F_GETFL);
    LOG_BRK_CLIENT_PERR(EXIT_CLIENT_F_SETFL);
    LOG_BRK_CLIENT_PERR(EXIT_CLIENT_SOCKET_OPEN_FAIL);
    LOG_BRK_CLIENT_FERR(EXIT_CLIENT_NO_HOST);
    LOG_BRK_CLIENT_FERR(EXIT_CLIENT_CONNECT_FAIL);
    LOG_BRK_CLIENT_FERR(EXIT_CLIENT_POLL_ERROR);
    LOG_BRK_CLIENT_NORM(EXIT_CLIENT_SERVER_CLOSE);
    close(sockfd);
    }
  }
}

void set_non_blocking(int sock) {
  int opts;
  opts = fcntl(sock, F_GETFL);
  if (opts < 0) exit(EXIT_CLIENT_F_GETFL);
  opts = (opts | O_NONBLOCK);
  if (fcntl(sock, F_SETFL, opts) < 0) exit(EXIT_CLIENT_F_SETFL);
}

int main() {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[BUF_SIZE];

  on_exit(client_exit_handler, &sockfd);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) exit(EXIT_CLIENT_SOCKET_OPEN_FAIL);

  server = gethostbyname("localhost");
  if (server == NULL) exit(EXIT_CLIENT_NO_HOST);

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr,
         server->h_addr, (size_t) server->h_length);
  serv_addr.sin_port = htons(PORT);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    exit(EXIT_CLIENT_CONNECT_FAIL);
  }

  set_non_blocking(sockfd);

  struct pollfd fds[2];
  fds[0].fd     = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd     = sockfd;
  fds[1].events = POLLIN;

  #define POLL_FOREVER -1
  while (true) {
    if (poll(fds, 2, POLL_FOREVER) < 0) exit(EXIT_CLIENT_POLL_ERROR);
    if (fds[0].revents & POLLIN) {
      memset(buffer, 0, BUF_SIZE);
      if (fgets(buffer, BUF_SIZE, stdin) == NULL) break;
      send(sockfd, buffer, strlen(buffer), 0);
    }
    if (fds[1].revents & POLLIN) {
      memset(buffer, 0, BUF_SIZE);
      int n = (int) recv(sockfd, buffer, BUF_SIZE - 1, 0);
      if (n > 0) printf("Received: %s", buffer);
      else if (n == 0) exit(EXIT_CLIENT_SERVER_CLOSE);
    }
  }
  return EXIT_SUCCESS;
}
