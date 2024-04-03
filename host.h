
#ifndef HOST_H_
#define HOST_H_

#include <sys/socket.h>
#include <stdint.h>
#include <stdbool.h>

// BEGIN: misc
#define PORT_DEFAULT 9001
#define MAX_DATA_LEN 256
uint16_t extract_or_default_port(int, char **);
// END: misc

// BEGIN: client
#define MAX_CLIENTS 3
#define MIN_NAME_LEN 3
#define MAX_NAME_LEN 25
// 15 minutes
#define TIMEOUT 900000

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

// BEGIN: net
int get_listener_socket(uint16_t);
void poll_disconnect_guard(int);
void connect_client(ClientPool *, int, struct sockaddr_storage *);
void disconnect_client(ClientPool *, int);
void broadcast_all(ClientPool *, int, int, char *, ssize_t);
// END: net

#endif
