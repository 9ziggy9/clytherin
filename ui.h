#include <ncurses.h>
#define KEY_ESC 27
#define KEY_DEL 127

void init_ncurses(void);
void init_colors(void);
void stdscr_border(void);
char *handle_input(WINDOW *);
void msg_post_to_feed(WINDOW *, const char *, int);

WINDOW *create_master_win(void);

void apply_border(int, int, int, int,
                  const char *, const char *,
                  const chtype, const chtype);

WINDOW *create_input_box(int, int);
void destroy_input_box(WINDOW *, WINDOW **);
