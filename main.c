#include "ui.h"
#include <assert.h>

int main(int argc, char *argv[]) {
  init_ncurses();

  // BEGIN: init colors
  assert(has_colors() && "Terminal does not support color.\n");

  start_color();
  init_pair(1, COLOR_YELLOW, COLOR_BLACK);
  init_pair(2, COLOR_GREEN, COLOR_BLACK);
  init_pair(3, COLOR_CYAN, COLOR_BLACK);
  init_pair(4, COLOR_RED, COLOR_BLUE);
  // END: init colors

  stdscr_border();

  int height  = getmaxy(stdscr) - 2;
  int width   = getmaxx(stdscr) - 2;
  int start_y = (LINES - height) / 2;
  int start_x = (COLS - width) / 2;

  WINDOW *w_master = create_master_win(height, width, start_y, start_x);

  int msg_id = 1;
  for (int i = 1; i < argc - 1; ++i) {
    mvwprintw(w_master, i, 1, " -> %s", argv[i]);
    msg_id++;
  }

  wattron(w_master, COLOR_PAIR(2) | A_BOLD | A_STANDOUT);
  mvwprintw(w_master, argc - 1, 1, " -> %s", argv[argc - 1]);
  wprintw(w_master, "    ");
  wattroff(w_master, COLOR_PAIR(2) | A_BOLD | A_STANDOUT);

  while (TRUE) {
    int key = wgetch(w_master);
    switch (key) {
    case 'q': case KEY_ESC: {
      exit_ncurses(w_master);
    } break;
    case KEY_F(1): {
      WINDOW *w_input = create_input_box(height, width, start_y, start_x);
      const char *msg_txt = handle_input(w_input);
      if (msg_txt != NULL) {
        msg_post_to_feed(w_master, msg_txt, ++msg_id);
        wrefresh(w_master);
      }
      destroy_input_box(w_master, &w_input);
    } break;
    default:
      break;
    }
  }

  return 0; // technically unreachable
}
