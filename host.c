#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <poll.h>

#include "host.h"
#include "log.h" // LOG_IMPLEMENTION defined in main.c

uint16_t extract_or_default_port(int argc, char **argv) {
  if (argc < 2) {
    LOG_FROM_WARN("no port given, defaulting to %d\n", PORT_DEFAULT);
    return PORT_DEFAULT;
  }
  return (uint16_t) atoi(argv[1]);
}

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
  // should I simply add 1 for listener?
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
      LOG_FROM_SUCC("added client on socket descriptor: %d\n", fd);
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
      LOG_FROM_SUCC("removed client on socket descriptor: %d\n", fd);
      LOG_APPEND("current connected clients: %d\n", pool->n_clients);
      return fd;
    }
  }
  LOG_FROM_ERR("client removal failed, "
               "socket descriptor %d not found\n", fd);
  return -1;
}

void clients_destroy(ClientPool *pool) {
  LOG_FROM_SUCC("freeing memory of ClientPool ...\n");
  free(pool->pfds);
  free(pool->clients);
  free(pool);
}

static const char *port_to_cstr(uint16_t port) {
  char *cstr = malloc(6); // 6 because uint16_t can be > 9999
  sprintf(cstr, "%d", port);
  return cstr;
}

static void addr_info_configure(struct addrinfo *config) {
  memset(config, 0, sizeof(*config)); // zero init all fields
  config->ai_family   = AF_UNSPEC;    // IPv4 and IPv6
  config->ai_socktype = SOCK_STREAM;  // TCP
  config->ai_flags    = AI_PASSIVE;   // assign addr of localhost for me
}

static void suppress_addr_in_use(int fd) {
  const int optval = 1; // setsockopt takes void * as opt value
  setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
}

int get_listener_socket(uint16_t port) {
  struct addrinfo config, *addr_info;
  addr_info_configure(&config);

  int rv; // store return value of getaddrinfo
  if ((rv = getaddrinfo(NULL, port_to_cstr(port), &config, &addr_info)) != 0)
  {
    LOG_FATAL("%s\n", gai_strerror(rv));
    exit(EXIT_FAILURE);
  }

  // traverse addr_info linked list
  int listener_fd;
  struct addrinfo *curr = addr_info;
  for (curr = addr_info; curr != NULL; curr = curr->ai_next) {
    listener_fd = socket(curr->ai_family, curr->ai_socktype, curr->ai_protocol);
    if (listener_fd < 0) continue;
    suppress_addr_in_use(listener_fd);
    if (bind(listener_fd, curr->ai_addr, curr->ai_addrlen) < 0) {
      close(listener_fd);
      continue;
    }
    break;
  }

  freeaddrinfo(addr_info);

  if (curr == NULL) {
    LOG_FATAL("failed to bind file descriptor\n");
    exit(EXIT_FAILURE);
  }

  if (listen(listener_fd, 10) == -1) {
    LOG_FATAL("failed to listen at file descriptor\n");
    exit(EXIT_FAILURE);
  }

  return listener_fd;
}

void poll_disconnect_guard(int poll_count) {
  switch (poll_count) {
  case 0:
    LOG_FATAL("polling timed out, do data for %d milliseconds\n", TIMEOUT);
    exit(EXIT_FAILURE);
  case -1:
    LOG_FATAL("poll suddenly dropped out\n");
    LOG_APPEND("errno: %s\n", strerror(errno));
    exit(EXIT_FAILURE);
  default: return;
  }
}

static void *get_in_addr(struct sockaddr *sa) {
  return (sa->sa_family == AF_INET)
    ? (void *) &(((struct sockaddr_in *) sa)->sin_addr)
    : (void *) &(((struct sockaddr_in6 *) sa)->sin6_addr);
}

void
connect_client(ClientPool *pool, int client_fd, struct sockaddr_storage *addr)
{
  char remoteIP[INET6_ADDRSTRLEN];
  if (client_fd == -1) {
    LOG_FROM_ERR("failed to accept client connection\n");
    LOG_APPEND("errno: %s\n", strerror(errno));
    return;
  }
  if (client_add(pool, client_fd, POLLIN) < 0) {
    LOG_FROM_ERR("failed to add client, closing socket\n");
    close(client_fd);
    return;
  }
  LOG_FROM_SUCC("new connection from %s on socket %d\n",
              inet_ntop(addr->ss_family,
                        get_in_addr((struct sockaddr *) addr),
                        remoteIP, INET6_ADDRSTRLEN),
              client_fd);
  return;
}

void disconnect_client(ClientPool *pool, int client_id) {
  close(pool->pfds[client_id].fd);
  client_remove(pool, pool->pfds[client_id].fd);
}

void broadcast_all(ClientPool *pool,
                   int send_fd, int list_fd,
                   char *msg, ssize_t msg_len)
{
  for (int c = 0; c < pool->n_clients; c++) {
    int dest_fd = pool->pfds[c].fd;
    if (dest_fd != list_fd && dest_fd != send_fd) { // exclude
      if (send(dest_fd, msg, (size_t) msg_len, 0) == -1) {
        LOG_FROM_ERR("send() failed\n");
        LOG_APPEND("errno: %s\n", errno);
      }
    }
  }
}
