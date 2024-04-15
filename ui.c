#include "ui.h"
#include <stdlib.h>
#include <string.h>

#define EXIT_UI_IMPL
#include "exit_handlers.h"

void init_ncurses(void) {
  initscr();
  noecho();
  cbreak();
  curs_set(FALSE);
  set_escdelay(0); // immediate escape key response
}

void init_colors(void) {
  if (!has_colors()) EXIT_WITH(EXIT_UI_NO_COLORS_IS_LAME);
  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_CYAN, COLOR_BLACK);
  init_pair(4, COLOR_RED, COLOR_BLUE);
}

static const char *master_toolbar_repr = "[ ESC >> QUIT ][ F1 >> SEND ]";
static const char *input_toolbar_repr  = "[ ESC >> CANCEL ][ ENTER >> SEND ]";

static void
write_toolbar_repr(WINDOW *win, int end, const char *fmt, const char *repr)
{
  int last_line, last_col;
  getmaxyx(win, last_line, last_col);
  mvwprintw(win, last_line - 1, last_col - end, fmt, repr);
}

static void
write_title_repr(WINDOW *win, const char *fmt, const char *title)
{
  mvwprintw(win, 0, 1, fmt, title);
}

void stdscr_border(void) {
  int txt_end = (int) strlen(master_toolbar_repr) + 3;

  attron(COLOR_PAIR(1));
  border(0,0,0,0,0,0,0,0);
  attroff(COLOR_PAIR(1));

  attron(A_BOLD);
  write_toolbar_repr(stdscr, txt_end, "%s", master_toolbar_repr);
  write_title_repr(stdscr, " %s ", "feed");
  attroff(A_BOLD);

  refresh();
}

char *handle_input(WINDOW *w_input) {
  char input_buffer[280];
  size_t i = 0;
  memset(input_buffer, 0, sizeof(input_buffer)); // zero out

  wmove(w_input, 0, 0);
  wclrtoeol(w_input);
  wrefresh(w_input);

  while (TRUE) {
    int key = wgetch(w_input);
    switch (key) {
    case KEY_F(1): case KEY_ESC: return NULL;
    case '\n': {
      char *input_str = (char *) malloc(i + 1);
      if (input_str == NULL) return NULL;
      memcpy(input_str, input_buffer, i + 1);
      return input_str;
    }
    case KEY_BACKSPACE:
    case KEY_DEL: if (i > 0) input_buffer[--i] = '\0'; break;
    default: if (i < sizeof(input_buffer) - 1) input_buffer[i++] = (char) key;
    }
    wmove(w_input, 0, 0);
    wclrtoeol(w_input);
    wprintw(w_input, "%s", input_buffer);
    wrefresh(w_input);
  }
}

void msg_post_to_feed(WINDOW *win, const char *txt, int id) {
  int width = getmaxx(win);
  wattron(win, COLOR_PAIR(1) | A_BOLD);
  mvwprintw(win, id, width - (int) (strlen(txt) + 5), " %s <- ", txt);
  wattroff(win, COLOR_PAIR(1) | A_BOLD);
}

WINDOW *create_master_win(void) {
  int height  = getmaxy(stdscr) - 2;
  int width   = getmaxx(stdscr) - 2;
  int start_y = (LINES - height) / 2;
  int start_x = (COLS - width) / 2;
  WINDOW *w_master = newwin(height, width, start_y, start_x);
  if (w_master == NULL) EXIT_WITH(EXIT_UI_NULL_MASTER);
  keypad(w_master, TRUE);
  wrefresh(w_master);
  return w_master;
}

void apply_border(int height, int width,
                  int start_y, int start_x,
                  const char *title,
                  const char *repr,
                  const chtype sym_y, const chtype sym_x)
{

  WINDOW *w_border = newwin(height + 2, width + 2, start_y - 1, start_x - 1);
  if (w_border == NULL) EXIT_WITH(EXIT_UI_NULL_BORDER);

  box(w_border, sym_y, sym_x);

  int txt_end = (int) strlen(repr) + 3;
  write_toolbar_repr(w_border, txt_end, "%s", repr);

  wattron(w_border, COLOR_PAIR(3) | A_BOLD | A_BLINK);
  write_title_repr(w_border, " %s ", title);
  wattroff(w_border, COLOR_PAIR(3) | A_BOLD | A_BLINK);

  wrefresh(w_border);
  delwin(w_border);
}

WINDOW *create_input_box(int lines, int cols)
{
  int height  = getmaxy(stdscr) - 2;
  int width   = getmaxx(stdscr) - 2;
  int start_y = (LINES - height) / 2;
  int start_x = (COLS - width) / 2;

  int input_start_y = start_y + (height - lines) / 2;
  int input_start_x = start_x + (width - cols) / 2;


  apply_border(lines, cols, input_start_y, input_start_x,
               "message", input_toolbar_repr, ACS_VLINE, ACS_HLINE);

  WINDOW *w_input = newwin(lines, cols, input_start_y, input_start_x);
  if (w_input == NULL) EXIT_WITH(EXIT_UI_NULL_INPUT);

  keypad(w_input, TRUE);
  curs_set(TRUE);
  wrefresh(w_input);

  return w_input;
}

void destroy_input_box(WINDOW *w_master, WINDOW **w_input) {
  delwin(*w_input);
  *w_input = NULL; 
  touchwin(w_master);
  curs_set(FALSE);
  wrefresh(w_master);
}
