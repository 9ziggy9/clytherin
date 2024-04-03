#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>

#define LOG_IMPLEMENTATION
#include "log.h"

// BEGIN: misc
#define PORT_DEFAULT 9001
#define MAX_TXT_BUFFER 256
uint16_t extract_or_default_port(int, char **);
// END: misc

// BEGIN: client
#define MAX_CLIENTS 2
#define MIN_NAME_LEN 3
#define MAX_NAME_LEN 25
#define TIMEOUT 2500

typedef struct {
  bool is_connected;
  const char *name;
  struct pollfd *pfd;
} Client;

typedef struct {
  uint8_t max;
  uint8_t n_clients;
  Client *clients;
  struct pollfd *pfds;
} ClientPool;

ClientPool *clients_init(uint8_t);
int client_add(ClientPool *, int, short);
int client_remove(ClientPool *, int);
void clients_destroy(ClientPool *);
// END: client

#ifndef TEST__ // PRODUCTION

int main(int argc, char **argv) {
  const uint16_t PORT = extract_or_default_port(argc, argv);
  (void) PORT;

  ClientPool *client_pool = clients_init(MAX_CLIENTS);
  client_add(client_pool, 0, POLLIN);

  LOG_FROM_STD("hit RETURN or wait 2.5 seconds for timeout\n");

  int n_events = poll(client_pool->pfds, client_pool->n_clients, TIMEOUT);

  if (n_events == 0) {
    LOG_FROM_ERR("poll timed out\n");
  } else {
    int ready_recv = client_pool->pfds[0].revents & POLLIN;
    if (ready_recv) {
      LOG_FROM_STD("file descriptor %d is ready to read\n",
                   client_pool->pfds[0].fd);
    } else {
      LOG_FROM_ERR("unexpected event occurred: %d\n",
                   client_pool->pfds[0].revents);
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




// BEGIN: IMPLEMENTATIONS
// BEGIN: MISC
uint16_t extract_or_default_port(int argc, char **argv) {
  if (argc < 2) {
    LOG_FROM_WARN("no port given, defaulting to %d\n", PORT_DEFAULT);
    return PORT_DEFAULT;
  }
  return (uint16_t) atoi(argv[1]);
}
// END: MISC

// BEGIN: CLIENTS
static void
pool_malloc_guard_fatal(ClientPool *p, Client *cs, struct pollfd *ps)
{
  const char *restrict fmt = "null pointer allocating %s\n";
  if (p == NULL) {
    LOG_FROM_ERR(fmt, "(ClientPool *)");
    exit(EXIT_FAILURE);
  }
  if (cs == NULL) {
    LOG_FROM_ERR(fmt, "(Client *)");
    exit(EXIT_FAILURE);
  }
  if (ps == NULL) {
    LOG_FROM_ERR(fmt, "(struct pollfd *)");
    exit(EXIT_FAILURE);
  }
}

ClientPool *clients_init(uint8_t max) {
  // do I need to add 1 for listener?
  ClientPool *pool     = malloc(sizeof(ClientPool));
  Client *clients      = malloc(max * sizeof(Client));
  struct pollfd *pfds  = malloc(max * sizeof(struct pollfd));
  pool_malloc_guard_fatal(pool, clients, pfds);

  pool->max = max;
  pool->n_clients = 0;
  pool->clients = clients;
  pool->pfds = pfds;

  for (int c = 0; c < max; c ++) {
    pool->pfds[c].fd = -1;
    pool->pfds[c].events = 0;
    pool->clients[c].is_connected = false;
    pool->clients[c].name = NULL;
    pool->clients[c].pfd = &pool->pfds[c];
  }

  return pool;
}

int client_add(ClientPool *pool, int fd, short ev_flags) {
  if (pool->n_clients >= pool->max) {
    LOG_FROM_ERR("max clients (%d) reached, add failed\n", pool->n_clients);
    return -1;
  }
  for (int c = 0; c < pool->max; c++) {
    if (!pool->clients[c].is_connected) {
      pool->pfds[c].fd = fd;
      pool->pfds[c].events = ev_flags;
      pool->clients[c].is_connected = true;
      pool->n_clients++;
      LOG_FROM_STD("added client on socket descriptor: %d\n", fd);
      LOG_APPEND("current connected clients: %d\n", pool->n_clients);
      return fd;
    }
  }
  LOG_FROM_ERR("could not find a free client slot\n"); // unreachable
  return -1;
}

int client_remove(ClientPool *pool, int fd) {
  if (pool->n_clients == 0) {
    LOG_FROM_ERR("no connected clients to remove\n");
    return -1;
  }
  for (int c = 0; c < pool->max; c++) {
    if (pool->pfds[c].fd == fd) {
      pool->pfds[c].fd = -1;
      pool->pfds[c].events = 0;
      pool->pfds[c].revents = 0;
      pool->clients[c].is_connected = false;
      pool->clients[c].name = NULL;
      pool->n_clients--;
      LOG_FROM_STD("removed client on socket descriptor: %d\n", fd);
      LOG_APPEND("current connected clients: %d\n", pool->n_clients);
      return fd;
    }
  }
  LOG_FROM_ERR("client removal failed, "
               "socket descriptor %d not found\n", fd);
  return -1;
}

void clients_destroy(ClientPool *pool) {
  LOG_FROM_STD("freeing memory of ClientPool ...\n");
  free(pool->pfds);
  free(pool->clients);
  free(pool);
}
// END: CLIENTS
// END: IMPLEMENTATIONS
