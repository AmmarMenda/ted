#include "ted.h"
#include <signal.h>
#include <termios.h>
#include <unistd.h>

static struct termios orig_termios;

static void disable_raw_mode(void) {
    tcsetattr(STDIN_FILENO, TCSAFLUSH, &orig_termios);
    fputs("\x1b[?2004l", stdout);
    fflush(stdout);
}
static void enable_raw_mode(void) {
    if (tcgetattr(STDIN_FILENO, &orig_termios) == -1) die("tcgetattr");
    atexit(disable_raw_mode);
    struct termios raw = orig_termios;
    raw.c_iflag &= ~(BRKINT | ICRNL | INPCK | ISTRIP | IXON);
    raw.c_oflag &= ~(OPOST);
    raw.c_cflag |=  (CS8);
    raw.c_lflag &= ~(ECHO | ICANON | IEXTEN | ISIG);
    raw.c_cc[VMIN] = 1; raw.c_cc[VTIME] = 0;
    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) == -1) die("tcsetattr");
    fputs("\x1b[?2004h", stdout);
    fflush(stdout);
}

int main(int argc, char *argv[]) {
    signal(SIGPIPE, SIG_IGN);
    signal(SIGCHLD, SIG_IGN);
    enable_raw_mode();
    syntax_init();
    Line first; line_init(&first);
    insert_line_at(0, first);
    cy = cx = 0; rowoff = coloff = 0;

    if (argc >= 2) {
        set_filename(argv[1]);
        if (editor_load(argv[1]) != 0) set_status("New file");
    }

    for (;;) {
        if (prompt_mode != PROMPT_NONE || search_active || !input_pending())
            editor_refresh();

        int c = read_key();
        if (c == KEY_NONE) continue;

        if (prompt_mode != PROMPT_NONE) { prompt_handle_key(c); continue; }
        if (search_active)              { search_handle_key(c); continue; }

        if (c == CTRL_KEY('f'))    { set_status(NULL); search_begin();                     continue; }
        if (c == CTRL_KEY('a'))    { set_status(NULL); sel_select_all();                   continue; }
        if (c == CTRL_KEY('z'))    { set_status(NULL); editor_undo();                      continue; }
        if (c == CTRL_KEY('y'))    { set_status(NULL); editor_redo();                      continue; }
        if (c == KEY_CTRL_SHIFT_C || c == KEY_CTRL_INSERT) {
            set_status(NULL); editor_copy_selection_to_system(); continue;
        }
        if (c == KEY_CTRL_SHIFT_V) { set_status(NULL); editor_paste_from_system();         continue; }
        if (c == KEY_PASTE)        { set_status(NULL);
                                     editor_insert_paste_buffer(paste_buf, paste_len);
                                     free(paste_buf); paste_buf = NULL; paste_len = 0;      continue; }

        if (sel_active) {
            switch (c) {
                case CTRL_KEY('v'):
                case KEY_ESC:       sel_active = 0;                             break;
                case CTRL_KEY('c'): editor_yank_selection();                    break;
                case KEY_BACKSPACE:
                case CTRL_KEY('h'):
                case KEY_DEL:       editor_delete_selection();                  break;
                case KEY_ENTER:     editor_delete_selection(); editor_newline(); break;
                case KEY_TAB:       editor_delete_selection(); editor_insert_char('\t'); break;
                case CTRL_KEY('q'): goto done;
                case CTRL_KEY('s'):
                    set_status(NULL);
                    prompt_begin(PROMPT_SAVE, "Save as: ", filename ? filename : "");
                    break;
                case CTRL_KEY('o'):
                    set_status(NULL);
                    prompt_begin(PROMPT_OPEN, "Open: ", "");
                    break;
                case KEY_LEFT:      cursor_left();      break;
                case KEY_RIGHT:     cursor_right();     break;
                case KEY_UP:        cursor_up();        break;
                case KEY_DOWN:      cursor_down();      break;
                case KEY_HOME:      cursor_home();      break;
                case KEY_END:       cursor_end();       break;
                case KEY_PGUP:      cursor_page_up();   break;
                case KEY_PGDN:      cursor_page_down(); break;
                case KEY_CTRL_UP:   cursor_top();       break;
                case KEY_CTRL_DOWN: cursor_bottom();    break;
                default:
                    if (c >= 32 && c < 127) {
                        editor_delete_selection();
                        editor_insert_char((char)c);
                    }
                    break;
            }
            continue;
        }

        switch (c) {
            case CTRL_KEY('q'): goto done;
            case CTRL_KEY('s'):
                set_status(NULL);
                prompt_begin(PROMPT_SAVE, "Save as: ", filename ? filename : "");
                break;
            case CTRL_KEY('o'):
                set_status(NULL);
                prompt_begin(PROMPT_OPEN, "Open: ", "");
                break;
            case CTRL_KEY('v'):
                sel_active     = 1;
                sel_anchor_cy  = cy;
                sel_anchor_cx  = cx;
                preferred_col  = -1;
                break;
            case KEY_ENTER:      set_status(NULL); editor_newline();         break;
            case KEY_TAB:        set_status(NULL); editor_insert_char('\t'); break;
            case KEY_BACKSPACE:
            case CTRL_KEY('h'):  set_status(NULL); editor_backspace();       break;
            case KEY_DEL:        set_status(NULL); editor_delete_forward();  break;
            case KEY_LEFT:       cursor_left();                              break;
            case KEY_RIGHT:      cursor_right();                             break;
            case KEY_UP:         cursor_up();                                break;
            case KEY_DOWN:       cursor_down();                              break;
            case KEY_HOME:       cursor_home();                              break;
            case KEY_END:        cursor_end();                               break;
            case KEY_PGUP:       cursor_page_up();                           break;
            case KEY_PGDN:       cursor_page_down();                         break;
            case KEY_CTRL_UP:    cursor_top();                               break;
            case KEY_CTRL_DOWN:  cursor_bottom();                            break;
            default:
                if (c >= 32 && c < 127) { set_status(NULL); editor_insert_char((char)c); }
                break;
        }
    }

done:
    search_clear_matches();
    undo_clear();
    syntax_shutdown();    
    fputs("\x1b[2J\x1b[H", stdout);
    fflush(stdout);
    free(filename);
    free(clipboard);
    free(paste_buf);
    return 0;
}