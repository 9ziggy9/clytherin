#include "ui.h"

int main(int argc, char *argv[]) {
  init_ncurses();
  init_colors();
  stdscr_border();

  WINDOW *w_master = create_master_win();

  // BEGIN: TESTING PURPOSES ONLY
  int msg_id = 1;
  for (int i = 1; i < argc - 1; ++i) {
    mvwprintw(w_master, i, 1, " -> %s", argv[i]);
    msg_id++;
 }
  // END: TESTING PURPOSES ONLY

  // TODO: Refactor, highlighting should be more generic
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
      WINDOW *w_input = create_input_box(6, 50);
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
