#ifndef EXIT_HANDLERS_H_
#define EXIT_HANDLERS_H_
#include <stdio.h>
#include <unistd.h>

#define LOG_BRK_NORM(MSG) case MSG: default: fprintf(stdout, ""#MSG"\n");
#define LOG_BRK_FERR(ERR) case ERR: fprintf(stderr, ""#ERR"\n"); break;
#define LOG_BRK_PERR(ERR) case ERR: perror(""#ERR"\n"); break;

#define EXIT_WITH(CODE)                            \
  do {                                             \
    fprintf(stderr, "EXIT :: %s() :: ", __func__); \
    exit(CODE);                                    \
  } while (0);                                     \

#ifdef EXIT_CLIENT_IMPL
typedef enum {
  EXIT_CLIENT_F_GETFL = 100    ,
  EXIT_CLIENT_F_SETFL          ,
  EXIT_CLIENT_SOCKET_OPEN_FAIL ,
  EXIT_CLIENT_NO_HOST          ,
  EXIT_CLIENT_CONNECT_FAIL     ,
  EXIT_CLIENT_POLL_ERROR       ,
  EXIT_CLIENT_SERVER_CLOSE     ,
} client_error_t;

#define CLIENT_ERR_RANGE                \
  exit_code >= EXIT_CLIENT_F_GETFL &&   \
  exit_code <= EXIT_CLIENT_SERVER_CLOSE \

void client_exit_handler(int exit_code, void *args) {
  int sockfd = *(int *) args;
  if (CLIENT_ERR_RANGE) {
    fprintf(stderr, "EXIT :: %s() ::\n", __func__);
    switch (exit_code) {
    LOG_BRK_PERR(EXIT_CLIENT_F_GETFL);
    LOG_BRK_PERR(EXIT_CLIENT_F_SETFL);
    LOG_BRK_PERR(EXIT_CLIENT_SOCKET_OPEN_FAIL);
    LOG_BRK_FERR(EXIT_CLIENT_NO_HOST);
    LOG_BRK_FERR(EXIT_CLIENT_CONNECT_FAIL);
    LOG_BRK_FERR(EXIT_CLIENT_POLL_ERROR);
    LOG_BRK_NORM(EXIT_CLIENT_SERVER_CLOSE);
    close(sockfd);
    }
  }
}
#endif // EXIT_CLIENT_IMPL

#ifdef EXIT_UI_IMPL
#include <ncurses.h>
typedef enum {
  EXIT_UI_NULL_MASTER = 200 ,
  EXIT_UI_NULL_BORDER       ,
  EXIT_UI_NULL_INPUT        ,
  EXIT_UI_NO_COLORS_IS_LAME ,
} ui_error_t;

#define UI_ERR_RANGE                     \
  exit_code >= EXIT_UI_NULL_MASTER &&    \
  exit_code <= EXIT_UI_NO_COLORS_IS_LAME \

void ui_exit_handler(int exit_code, void *args) {
  (void) args;
  if (UI_ERR_RANGE) {
    switch (exit_code) {
    LOG_BRK_FERR(EXIT_UI_NULL_MASTER);
    LOG_BRK_FERR(EXIT_UI_NULL_BORDER);
    LOG_BRK_FERR(EXIT_UI_NULL_INPUT);
    LOG_BRK_FERR(EXIT_UI_NO_COLORS_IS_LAME);
    default: break;
    }
  }
  endwin();
}
#endif // EXIT_UI_IMPL

#endif // EXIT_HANDLERS_H_
