#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <errno.h>

#include "db.h"

#define PORT_DEFAULT 9001
#define MAX_TXT_BUFFER 256

// BEGIN: client data structure
// In the future, a thread pool should be implemented.
#define MAX_CLIENTS 2
int CLIENT_COUNT = 0;

typedef struct {
  bool is_connected;
  const char *name;
  int id;
  int socket;
  pthread_t thread_id;
} Client;

Client CLIENTS[MAX_CLIENTS];
void clients_init(void);
void add_client(pthread_t, int);
void remove_client(int socket);
// END: client data structure

// BEGIN: signal thread
typedef struct {
  sigset_t *set;
  int *host_socket;
} ThreadContext;
sigset_t init_sigset(void);
void *handle_signal_thread(void *);
// END: signal thread

/*
Setting RUN as volatile sig_atomic_t ensures atomicity for int-sized read/write
but does not prevent potential read-modify-write race conditions.
For better thread synchronization, consider using mutexes or condition variables
alongside the RUN flag.
*/
volatile sig_atomic_t RUN = 1;

void join_all_threads(void);
void *handle_client(void *);
void host_panic_on_fail(bool, int, const char *);
int resolve_client_connection(int, pthread_t *, struct sockaddr_in);
uint16_t extract_or_default_port(int, char **);
void server_addr_init(struct sockaddr_in *server_addr, uint16_t);

const char *WELCOME_MSG = "hello, welcome fren!\n";
const char *CONN_REFUSED = "Connection to host refused!\n";
 
int main(int argc, char **argv) {
  const uint16_t PORT = extract_or_default_port(argc, argv);

  int host_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  // BEGIN: HOST SOCKET
  host_socket = socket(AF_INET, SOCK_STREAM, 0);
  host_panic_on_fail(host_socket < 0,
                     host_socket,
                     "failed to create socket.\n");
  // TCP TIME_WAIT state may linger, this comes from TCP stack configuration
  // of the operating system, setting SO_REUSEADDR is to mitigate busy port
  // annoyances.
  int optval = 1;
  setsockopt(host_socket, SOL_SOCKET, SO_REUSEADDR, &optval, sizeof(optval));
  fcntl(host_socket, F_SETFL, O_NONBLOCK); // non-blocking socket
  // END: HOST SOCKET

  server_addr_init(&server_addr, PORT);

  const int bind_result = bind(host_socket,
                                (struct sockaddr *) &server_addr,
                                sizeof(server_addr));

  host_panic_on_fail(bind_result < 0,
                     host_socket,
                     "binding on socket failed.\n");

  host_panic_on_fail(listen(host_socket, 5) < 0,
                     host_socket,
                     "failed to listen on socket.\n");

  printf("Server listening on port %d...\n", PORT);

  // BEGIN: logic for signal thread blocker
  sigset_t set = init_sigset();
  ThreadContext ctx = {
    .set = &set,
    .host_socket = &host_socket
  };
  pthread_t sig_thread = 0;
  pthread_create(&sig_thread, NULL, handle_signal_thread, (void *) &ctx);
  // END: logic for signal thread blocker

  clients_init(); // to unconnected

  while (RUN) {
    int client_socket = accept(host_socket,
                              (struct sockaddr *) &client_addr,
                              &client_addr_len);
    if (client_socket < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        /*
          This approach is functional but inefficient.
          Consider using select(), poll(), or epoll() for a more efficient
          wait mechanism that can handle multiple descriptors
          (client sockets and possibly other resources) simultaneously without
          busy waiting.
         */
        sleep(1);
        continue;
      } else if (RUN) {
        perror("HOST :: rejected client_socket.\n");
        continue;
      }
    }

    pthread_t thread = 0;
    resolve_client_connection(client_socket, &thread, client_addr);
  }

  close(host_socket);
  join_all_threads();
  printf("Host closed peacefully.\n");
  return 0;
}

void host_panic_on_fail(bool cond, int host_socket, const char *msg) {
  if (cond) {
    perror("HOST ::");
    fprintf(stderr, "%s(): %s", __func__, msg);
    close(host_socket);
    exit(EXIT_FAILURE);
  }
}

void broadcast_message_from(int sender_socket, const char *msg) {
  for (int i = 0; i < CLIENT_COUNT; i++) {
    if (CLIENTS[i].socket != sender_socket) {
      send(CLIENTS[i].socket, msg, strlen(msg), 0);
    }
  }
}

void join_all_threads(void) {
  printf("%s(): cleaning up threads.\n", __func__);
  for (int i = 0; i < CLIENT_COUNT; i++) {
    if (pthread_cancel(CLIENTS[i].thread_id) == 0) {
      printf("%s(): cancelling thread %d\n", __func__, i);
      fflush(stdout);
      pthread_join(CLIENTS[i].thread_id, NULL);
    }
  }
  CLIENT_COUNT = 0;
}

int resolve_client_connection(int client_socket,
                              pthread_t *thread,
                              struct sockaddr_in client_addr)
{
  if (client_socket < 0) return -1;
  if ((CLIENT_COUNT + 1 > MAX_CLIENTS)) {
    printf("%s(): rejected new client, at (%d) capacity.\n",
           __func__, MAX_CLIENTS);

    send(client_socket, CONN_REFUSED, strlen(CONN_REFUSED), 0);
    close(client_socket);
    return -1;
  };

  int *ptr_client_socket = malloc(sizeof(int));
  if (!ptr_client_socket) {
    fprintf(stderr, "%s(): malloc failure.\n", __func__);
    close(client_socket);
    return -1;
  }

  *ptr_client_socket = client_socket;
  if (pthread_create(thread, NULL, handle_client, ptr_client_socket) != 0) {
    close(client_socket);
    free(ptr_client_socket); // Cleanup on thread creation failure
    return -1;
  }

  add_client(*thread, client_socket);
  printf("%s(): client connected --> %s\nTotal clients: %d\n",
         __func__, inet_ntoa(client_addr.sin_addr), CLIENT_COUNT);
  send(client_socket, WELCOME_MSG, strlen(WELCOME_MSG), 0);
  return 0;
}

void client_cleanup_handler(void *arg) {
  int client_socket = *((int *) arg);
  printf("%s(): client (%d) exiting ...\n", __func__, client_socket);
  printf("%s(): cleaning client socket ... \n", __func__);
  fflush(stdout);
  close(client_socket);
  remove_client(client_socket);
  free(arg);
}

void *handle_client(void *arg) {
  int *client_socket = (int *) arg;
  pthread_cleanup_push(client_cleanup_handler, client_socket);

  char buffer[MAX_TXT_BUFFER];
  ssize_t bytes_read;

  while (RUN) {
    // should switch to select(), poll() or epoll()
    bytes_read = recv(*client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) break;
    buffer[bytes_read++] = '\n'; // Null-terminate the received data
    buffer[bytes_read] = '\0'; // Null-terminate the received data
    printf("Received: %s", buffer);
    send(*client_socket, "msg sent", strlen("msg sent"), 0);
    broadcast_message_from(*client_socket, buffer);
  }

  close(*client_socket);
  pthread_cleanup_pop(1);
  return NULL;
}

uint16_t extract_or_default_port(int argc, char **argv) {
  if (argc < 2) {
    printf("WARNING: no port provided, defaulting to %d.\n", PORT_DEFAULT);
    return PORT_DEFAULT;
  }
  return (uint16_t) atoi(argv[1]);
}

void server_addr_init(struct sockaddr_in *addr, uint16_t port) {
  memset(addr, 0, sizeof(*addr));
  addr->sin_family      = AF_INET;
  addr->sin_addr.s_addr = INADDR_ANY;
  addr->sin_port        = htons(port);
}

/* init_sigset does two things: it initializes a sigset_t with specific signals
   (SIGINT and SIGTERM) and applies this set to block these signals in the
   calling thread (and subsequently all threads created after this call due to
   thread inheritance of signal masks). */
sigset_t init_sigset(void) {
  sigset_t set;
  sigemptyset(&set);
  sigaddset(&set, SIGINT);
  sigaddset(&set, SIGTERM);
  pthread_sigmask(SIG_BLOCK, &set, NULL);
  return set;
}

// Note that void pointer essentially allows the fn to be generic!
void *handle_signal_thread(void *raw) {
  ThreadContext *ctx = (ThreadContext *) raw;
  int sig;
  for (;;) {
    sigwait(ctx->set, &sig); // wait for any signal in set
    if (sig == SIGINT || sig == SIGTERM) {
      printf("\nCaught interrupt signal %d\n", sig);
      RUN = 0;
      printf("Shutting down...\n");
      break;
    }
  }
  return NULL;
}

void clients_init(void) {
  size_t n = 0;
  while (n < MAX_CLIENTS) {
    CLIENTS[n].is_connected = false;
    CLIENTS[n].socket = -1;
    CLIENTS[n++].id = -1;
  }
}

void add_client(pthread_t thread, int client_socket) {
  CLIENTS[CLIENT_COUNT].thread_id = thread;
  CLIENTS[CLIENT_COUNT].socket = client_socket;
  CLIENTS[CLIENT_COUNT].id = CLIENT_COUNT;
  CLIENTS[CLIENT_COUNT].is_connected = true;
  CLIENT_COUNT++;
  printf("%s(): client successfully added on socket %d.\n",
         __func__, client_socket);
  fflush(stdout);
}

void remove_client(int socket) {
  for (size_t n = 0; n < MAX_CLIENTS; n ++) {
    if (CLIENTS[n].socket == socket) {
      CLIENTS[n].socket = -1;
      CLIENTS[n].id = -1;
      CLIENTS[n].is_connected = false;
      CLIENT_COUNT--;
      printf("%s(): client successfully removed from socket %d.\n",
             __func__, socket);
      fflush(stdout);
    }
  }
}
