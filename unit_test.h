#define UNIT_CLIENT_BASICS()                           \
do {                                                   \
  ClientPool *client_pool = clients_init(MAX_CLIENTS); \
  client_remove(client_pool, 0);                       \
  client_add(client_pool, 0, POLLIN | POLLOUT);        \
  client_add(client_pool, 0, POLLIN | POLLOUT);        \
  client_add(client_pool, 0, POLLIN | POLLOUT);        \
  client_remove(client_pool, 0);                       \
  client_remove(client_pool, -100);                    \
  client_remove(client_pool, 0);                       \
  clients_destroy(client_pool);                        \
} while(0)
