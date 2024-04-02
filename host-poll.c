#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <stdbool.h>
#include <poll.h>
#include <stdio.h>
#include <stdarg.h>

#define PORT_DEFAULT 9001
#define MAX_TXT_BUFFER 256

// BEGIN: client data structure
#define MAX_CLIENTS 2
int CLIENT_COUNT = 0;

typedef struct {
  bool is_connected;
  const char *name;
  struct pollfd fd;
} Client;

Client CLIENTS[MAX_CLIENTS];
void clients_init(void);
void add_client(pthread_t, int);
void remove_client(int socket);
// END: client data structure


uint16_t extract_or_default_port(int, char **);

// BEGIN: LOGGING FN
// While you can use the implementation function directly, the macro
// is written to avoid direct usage of __func__ every time we call a log.
typedef enum { STD, WARN, ERR } log_t ;
#define LOG_FROM_STD(fmt, ...) \
  _log_from_fn(STD, __func__, fmt, ##__VA_ARGS__)
#define LOG_FROM_WARN(fmt, ...) \
  _log_from_fn(WARN, __func__, fmt, ##__VA_ARGS__)
#define LOG_FROM_ERR(fmt, ...) \
  _log_from_fn(ERR, __func__, fmt, ##__VA_ARGS__)

void _log_from_fn(log_t target, const char *fn, const char *fmt, ...) {
  FILE *stream = target == STD ? stdout : stderr;

  const char *ansi_clr = target == STD
    ? "\033[32m" : target == WARN
    ? "\033[33m" : "\033[31m";

  switch (target) {
  case STD:
    fprintf(stream,  "%s[MESSAGE]\033[0m:: %s()\n", ansi_clr, fn); break;
  case WARN:
    fprintf(stream,  "%s[WARNING]\033[0m :: %s()\n", ansi_clr, fn); break;
  case ERR:
    fprintf(stream,  "%s[ERROR]\033[0m :: %s()\n",   ansi_clr, fn); break;
  default:
    fprintf(stderr,  "[UNKNOWN] :: unreachable case\n"); return;
  }

  va_list args;
  va_start(args, fmt);
  fprintf(stream, " - ");
  vfprintf(stream, fmt, args);
  va_end(args);
}
// END: LOGGING FN

// BEGIN: signal thread
typedef struct {
  sigset_t *set;
  int *host_socket;
} ThreadContext;
sigset_t init_sigset(void);
void *handle_signal_thread(void *);
// END: signal thread

int main(int argc, char **argv) {
  const uint16_t PORT = extract_or_default_port(argc, argv);
  (void) PORT;
  return EXIT_SUCCESS;
}

uint16_t extract_or_default_port(int argc, char **argv) {
  if (argc < 2) {
    LOG_FROM_WARN("no port given, defaulting to %d\n", PORT_DEFAULT);
    return PORT_DEFAULT;
  }
  return (uint16_t) atoi(argv[1]);
}
