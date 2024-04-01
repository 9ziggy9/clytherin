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

#define PORT_DEFAULT 9001
#define MAX_CLIENTS 2
pthread_t CLIENT_THREAD_IDS[MAX_CLIENTS];
int CLIENT_COUNT = 0;

typedef struct {
  sigset_t *set;
  int *server_socket;
} ThreadContext;

volatile sig_atomic_t RUN = 1;
sigset_t init_sigset(void);
void *handle_signal_thread(void *);

void join_all_threads(void);
void add_thread(pthread_t);
void *handle_client(void *);
void host_panic_on_fail(bool, int, const char *);
int resolve_client_connection(int, pthread_t *, struct sockaddr_in);
uint16_t extract_or_default_port(int, char **);
void server_addr_init(struct sockaddr_in *server_addr, uint16_t);
 
int main(int argc, char **argv) {
  const uint16_t PORT = extract_or_default_port(argc, argv);

  int server_socket;
  struct sockaddr_in server_addr, client_addr;
  socklen_t client_addr_len = sizeof(client_addr);

  server_socket = socket(AF_INET, SOCK_STREAM, 0);
  host_panic_on_fail(server_socket < 0,
                     server_socket,
                     "failed to create socket.\n");

  fcntl(server_socket, F_SETFL, O_NONBLOCK);

  server_addr_init(&server_addr, PORT);

  const int bind_result = bind(server_socket,
                                (struct sockaddr *) &server_addr,
                                sizeof(server_addr));

  host_panic_on_fail(bind_result < 0,
                     server_socket,
                     "binding on socket failed.\n");

  host_panic_on_fail(listen(server_socket, 5) < 0,
                     server_socket,
                     "failed to listen on socket.\n");

  printf("Server listening on port %d...\n", PORT);

  // BEGIN: logic for signal thread blocker
  sigset_t set = init_sigset();
  ThreadContext ctx = {
    .set = &set,
    .server_socket = &server_socket
  };
  pthread_t sig_thread = 0;
  pthread_create(&sig_thread, NULL, handle_signal_thread, (void *) &ctx);
  // END: logic for signal thread blocker

  while (RUN) {
    int client_socket = accept(server_socket,
                              (struct sockaddr *) &client_addr,
                              &client_addr_len);
    if (client_socket < 0) {
      if (errno == EWOULDBLOCK || errno == EAGAIN) {
        sleep(1); // this is a bit of a hack, look into self-piping
        continue;
      } else if (RUN) {
        perror("HOST :: rejected client_socket.\n");
        continue;
      }
    }

    pthread_t thread = 0;
    if (resolve_client_connection(client_socket,
                                  &thread, client_addr) != 0 && RUN)
    {
      fprintf(stderr, "HOST :: %s(): failed to resolve client.\n", __func__);
    }
  }

  close(server_socket);
  join_all_threads();
  printf("Host closed peacefully.\n");
  return 0;
}

void host_panic_on_fail(bool cond, int server_socket, const char *msg) {
  if (cond) {
    perror(strcat("HOST :: panic_on_fail(): ", msg));
    close(server_socket);
    exit(EXIT_FAILURE);
  }
}

void add_thread(pthread_t thread) {
  if (CLIENT_COUNT < MAX_CLIENTS) {
    CLIENT_THREAD_IDS[CLIENT_COUNT++] = thread;
    printf("Client thread successfully added.\n");
  } else {
    fprintf(stderr, "%s(): maximum thread count met.\n", __func__);
  }
}

void join_all_threads(void) {
  printf("%s(): cleaning up threads.\n", __func__);
  for (int i = 0; i < CLIENT_COUNT; i++) {
    if (pthread_cancel(CLIENT_THREAD_IDS[i]) == 0) {
      pthread_join(CLIENT_THREAD_IDS[i], NULL);
    }
  }
  CLIENT_COUNT = 0;
}

int resolve_client_connection(int client_socket,
                              pthread_t *thread,
                              struct sockaddr_in client_addr)
{
  if (client_socket < 0) return -1;
  else {
    int *ptr_client_socket = malloc(sizeof(int));
    if (ptr_client_socket == NULL) {
      fprintf(stderr, "%s(): malloc failure.\n", __func__);
      close(client_socket);
      return -1;
    } else {
      *ptr_client_socket = client_socket;
      int p = pthread_create(thread, NULL, handle_client, ptr_client_socket);
      if (p == 0) {
        add_thread(*thread); 
      } else {
        close(client_socket);
        free(ptr_client_socket);
        return -1;
      }
    }
  }
  printf("Client connected: %s\n", inet_ntoa(client_addr.sin_addr));
  return 0;
}

void *handle_client(void *arg) {
  int client_socket = *((int *) arg);

  char buffer[256];
  int bytes_read;

  while (RUN) {
    // should switch to select(), poll() or epoll()
    bytes_read = (int) recv(client_socket, buffer, sizeof(buffer) - 1, 0);
    if (bytes_read <= 0) break;
    buffer[bytes_read] = '\0'; // Null-terminate the received data
    printf("Received: %s", buffer);
  }

  close(client_socket);
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
