/*
  This is an stb-style header only library, you must define LOG_IMPLEMENTATION
  BEFORE including the header-file. i.e.,

  #define LOG_IMPLEMENTATION
  #include "log.h"

  You may then simply `#include "log.h"` in any source files it is needed.
 */

#ifndef LOG_H_
#define LOG_H_

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

typedef enum { SUCC, WARN, ERR, FATAL } log_t;
void _log_append(const char *fmt, ...);
void _log_from_fn(log_t target, const char *fn, const char *fmt, ...);

#define LOG_FROM_SUCC(fmt, ...) _log_from_fn(SUCC, __func__, fmt, ##__VA_ARGS__)
#define LOG_FROM_WARN(fmt, ...) _log_from_fn(WARN, __func__, fmt, ##__VA_ARGS__)
#define LOG_FROM_ERR(fmt, ...)  _log_from_fn(ERR, __func__, fmt, ##__VA_ARGS__)
#define LOG_FATAL(fmt, ...)     _log_from_fn(FATAL,__func__, fmt, ##__VA_ARGS__)
#define LOG_APPEND(fmt, ...)    _log_append(fmt, ##__VA_ARGS__)

#ifdef LOG_IMPLEMENTATION
static FILE *_log_stream = NULL;

void _log_append(const char *fmt, ...) {
  fprintf(_log_stream, "» ");
  va_list args;
  va_start(args, fmt);
  vfprintf(_log_stream, fmt, args);
  va_end(args);
}

void _log_from_fn(log_t target, const char *fn, const char *fmt, ...)
{
  _log_stream = target == SUCC ? stdout : stderr;

  const char *ansi_clr = target == SUCC
    ? "\033[32m" : target == WARN
    ? "\033[33m" : "\033[31m";

  switch (target) {
  case SUCC:
    fprintf(_log_stream,  "\n%s[SUCCESS]\033[0m :: %s()\n",  ansi_clr, fn);
    break;
  case WARN:
    fprintf(_log_stream,  "\n%s[WARNING]\033[0m :: %s()\n", ansi_clr, fn);
    break;
  case ERR:
    fprintf(_log_stream,  "\n%s[ERROR]\033[0m :: %s()\n",   ansi_clr, fn);
    break;
  case FATAL:
    fprintf(_log_stream,  "\n%s!! [FATAL] !!\033[0m :: "
                          "exiting immediately :: %s()\n", ansi_clr, fn);
    break;
  default:
    fprintf(stderr,  "[UNKNOWN] :: unreachable case\n");
    return;
  }
  fprintf(_log_stream, "» ");
  va_list args;
  va_start(args, fmt);
  vfprintf(_log_stream, fmt, args);
  va_end(args);
}

#endif // LOG_IMPLEMENTATION
#endif //  LOG_H_
