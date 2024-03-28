#ifndef UI_H
#define UI_H

#include <ncurses.h>
/* #include <stdlib.h> */
/* #include <string.h> */

#define KEY_ESC 27
#define KEY_DEL 127

void panic_null_win(WINDOW *);
void exit_ncurses(WINDOW *);

void init_ncurses(void);
void stdscr_border(void);

WINDOW *create_master_win(int, int, int, int);

WINDOW *create_input_box(int, int, int, int);
void destroy_input_box(WINDOW *, WINDOW **);
char *handle_input(WINDOW *);
void apply_border(int, int, int, int,
                  const char *, const char *,
                  const chtype,   const chtype);

void msg_post_to_feed(WINDOW *, const char *, int);

extern const char *master_toolbar_txt;
extern const char *input_toolbar_txt;

#endif // UI_H
