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

#define EXIT_CLIENT_IMPL
#include "exit_handlers.h"
#include "ui.h"

#define PORT     9001
#define BUF_SIZE 1024

void set_non_blocking(int sock) {
  int opts;
  opts = fcntl(sock, F_GETFL);
  if (opts < 0) EXIT_WITH(EXIT_CLIENT_F_GETFL);
  opts = (opts | O_NONBLOCK);
  if (fcntl(sock, F_SETFL, opts) < 0) EXIT_WITH(EXIT_CLIENT_F_SETFL);
}

int main() {
  int sockfd;
  struct sockaddr_in serv_addr;
  struct hostent *server;
  char buffer[BUF_SIZE];

  on_exit(client_exit_handler, &sockfd);

  sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) EXIT_WITH(EXIT_CLIENT_SOCKET_OPEN_FAIL);

  server = gethostbyname("localhost");
  if (server == NULL) EXIT_WITH(EXIT_CLIENT_NO_HOST);

  memset(&serv_addr, 0, sizeof(serv_addr));
  serv_addr.sin_family = AF_INET;
  memcpy(&serv_addr.sin_addr.s_addr,
         server->h_addr, (size_t) server->h_length);
  serv_addr.sin_port = htons(PORT);

  if (connect(sockfd, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
    EXIT_WITH(EXIT_CLIENT_CONNECT_FAIL);
  }

  set_non_blocking(sockfd);

  struct pollfd fds[2];
  fds[0].fd     = STDIN_FILENO;
  fds[0].events = POLLIN;
  fds[1].fd     = sockfd;
  fds[1].events = POLLIN;

// BEGIN: CURSES
  init_ncurses();
  init_colors();
  stdscr_border();
  WINDOW *w_master = create_master_win();

  #define POLL_FOREVER -1
  int msg_id = 1;
  while (TRUE) {
    if (poll(fds, 2, POLL_FOREVER) < 0) EXIT_WITH(EXIT_CLIENT_POLL_ERROR);
    if (fds[1].revents & POLLIN) {
      memset(buffer, 0, BUF_SIZE);
      int n = (int) recv(sockfd, buffer, BUF_SIZE - 1, 0);
      if (n > 0) mvwprintw(w_master, 1, 1, buffer);
      else if (n == 0) EXIT_WITH(EXIT_CLIENT_SERVER_CLOSE);
    }
    switch (wgetch(w_master)) {
    case 'q': case KEY_ESC: exit(EXIT_SUCCESS);
    case KEY_F(1): {
      WINDOW *w_input     = create_input_box(6, 50);
      const char *msg_txt = handle_input(w_input);
      if (msg_txt != NULL) {
        msg_post_to_feed(w_master, msg_txt, ++msg_id);
        /* if (fds[0].revents & POLLIN) { */
        /*   memset(buffer, 0, BUF_SIZE); */
        /*   if (fgets(buffer, BUF_SIZE, stdin) == NULL) break; */
        /*   send(sockfd, buffer, strlen(buffer), 0); */
        /* } */
        wrefresh(w_master);
      }
      destroy_input_box(w_master, &w_input);
    } break;
    default:
      break;
    }
  }
  return EXIT_SUCCESS;
}
