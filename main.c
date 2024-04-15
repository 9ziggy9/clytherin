#include <poll.h>
#include "host.h"
#define LOG_IMPLEMENTATION
#include "log.h"

#ifndef TEST__ // PRODUCTION
int main(int argc, char **argv) {
  const uint16_t port = extract_or_default_port(argc, argv);
  char data_buffer[MAX_DATA_LEN];

  ClientPool *client_pool = clients_init(MAX_CLIENTS);

  int listener = get_listener_socket(port);
  client_add(client_pool, listener, POLLIN);

  for (;;) {
    int poll_count = poll(client_pool->pfds, client_pool->n_clients, TIMEOUT);
    poll_disconnect_guard(poll_count);

    for (int c = 0; c < client_pool->n_clients; c++) {
      if (client_pool->pfds[c].revents && POLLIN) {
        if (client_pool->pfds[c].fd == listener) { // listener routines
          struct sockaddr_storage client_addr;
          socklen_t client_addr_len = sizeof(client_addr);
          int new_client_fd = accept(listener, (struct sockaddr *) &client_addr,
                                     &client_addr_len);
          connect_client(client_pool, new_client_fd, &client_addr);
        } else { // client routines
          ssize_t num_bytes = recv(client_pool->pfds[c].fd,
                                   data_buffer, sizeof(data_buffer), 0);
          if (num_bytes <= 0) {
            if (num_bytes == 0) {
              LOG_FROM_SUCC("socket %d hung up\n", client_pool->pfds[c].fd);
            } else {
              LOG_FROM_ERR("recv() failure\n");
              LOG_APPEND("errno: %s\n", errno);
            }
            disconnect_client(client_pool, c);
          } else {
            broadcast_all(client_pool,
                          client_pool->pfds[c].fd,
                          listener,
                          data_buffer, num_bytes);
          }
        }
      }
    }
  }
  return EXIT_SUCCESS;
}
#else // END PRODUCTION
#include "unit_test.h"

int main(int argc, char **argv) {
  (void) argc; (void) argv;
  UNIT_CLIENT_BASICS();
  return EXIT_SUCCESS;
}
#endif
