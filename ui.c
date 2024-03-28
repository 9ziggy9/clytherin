#include "ui.h"
#include <stdlib.h>
#include <string.h>
#include <assert.h>

const char *master_toolbar_txt = "<esc | q: quit> <f1: new message>";
const char *input_toolbar_txt = "<esc | f1: cancel> <enter: send>";

void panic_null_win(WINDOW *w) {
  if (w == NULL) {
    endwin();
    fprintf(stderr, "PANIC: <panic_null_win()> NULL window returned.\n");
    exit(EXIT_FAILURE);
  }
}

void init_ncurses(void) {
  initscr();
  noecho();
  cbreak();
  curs_set(FALSE);
  set_escdelay(0); // immediate escape key response
}

void init_colors(void) {
  assert(has_colors() && "Terminal does not support color.\n");
  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_CYAN, COLOR_BLACK);
  init_pair(4, COLOR_RED, COLOR_BLUE);
}

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
  // stdscr border and tools bars
  int txt_end = (int) strlen(master_toolbar_txt) + 3;

  attron(COLOR_PAIR(1));
  border(0,0,0,0,0,0,0,0);
  attroff(COLOR_PAIR(1));

  attron(A_BOLD);
  write_toolbar_repr(stdscr, txt_end, " %s ", master_toolbar_txt);
  write_title_repr(stdscr, " %s ", "feed");
  attroff(A_BOLD);

  refresh();
}

void exit_ncurses(WINDOW *w) {
  delwin(w);
  endwin();
  exit(EXIT_SUCCESS);
}

char *handle_input(WINDOW *w_input) {
  char input_buffer[280];
  size_t i = 0;
  memset(input_buffer, 0, sizeof(input_buffer)); // zero out

  wmove(w_input, 0, 0); // relative to window position! :)
  wclrtoeol(w_input);   // clear to end of line
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
    case KEY_BACKSPACE: case KEY_DEL: {
      if (i > 0) input_buffer[--i] = '\0';
    } break;
    default:  {
      if (i < sizeof(input_buffer) - 1) input_buffer[i++] = (char) key;
    } break;
    }
    wmove(w_input, 0, 0); // Move cursor back to the start of the input line
    wclrtoeol(w_input); // Clear the line
    wprintw(w_input, "%s", input_buffer); // Print the current buffer content
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
  panic_null_win(w_master);
  keypad(w_master, TRUE);
  wrefresh(w_master);
  return w_master;
}


/*
  :: NOTES: apply_border() ::
  Not sure how smart this is. We refresh once padding is created, then
  IMMEDIATELY delete, so that refreshes of embedded windows do not
  alter the deleted padding. subwin is no good because then I still
  need a reference to it, as it must be deleted BEFORE the originator.
  Probably best to just use a product type here, i.e.:

  typedef struct {
    WINDOW *w_input;
    WINDOW *w_border;
  } INPUT_WIN;

  Then ammend destroy function as needed.
*/
void apply_border(int height, int width,
                  int start_y, int start_x,
                  const char *title,
                  const char *tool_txt,
                  const chtype sym_y, const chtype sym_x)
{

  WINDOW *w_border = newwin(height + 2, width + 2, start_y - 1, start_x - 1);
  panic_null_win(w_border);

  box(w_border, sym_y, sym_x);

  int txt_end = (int) strlen(tool_txt) + 1;
  write_toolbar_repr(w_border, txt_end, " %s ", tool_txt);

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
               "message", input_toolbar_txt,
               ACS_VLINE, ACS_HLINE);

  WINDOW *w_input = newwin(lines, cols, input_start_y, input_start_x);
  panic_null_win(w_input);

  keypad(w_input, TRUE);
  curs_set(TRUE);
  wrefresh(w_input);

  return w_input;
}

// Double pointer to w_input necessary to prevent pointer from being passed
// by value. If this were not used, then setting w_input = NULL would never
// actually alter the value of w_input.
void destroy_input_box(WINDOW *w_master, WINDOW **w_input) {
  delwin(*w_input);
  *w_input = NULL; 
  touchwin(w_master);
  curs_set(FALSE);
  wrefresh(w_master);
}
