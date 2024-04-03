#ifndef UI_H
#define UI_H

#include <ncurses.h>

#define KEY_ESC 27
#define KEY_DEL 127

#define MAX_BINDINGS 12

void panic_null_win(WINDOW *);
void exit_ncurses(WINDOW *);

void init_ncurses(void);
void init_colors(void);
void stdscr_border(void);

WINDOW *create_master_win(void);
WINDOW *create_input_box(int, int);

void destroy_input_box(WINDOW *, WINDOW **);
char *handle_input(WINDOW *);
void apply_border(int, int, int, int,
                  const char *, const char *,
                  const chtype, const chtype);

void msg_post_to_feed(WINDOW *, const char *, int);

#endif // UI_H
