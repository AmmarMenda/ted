#include "ted.h"
#include <unistd.h>
#include <sys/ioctl.h>

static void get_window_size(int *rows, int *cols) {
    struct winsize ws;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == -1 || ws.ws_row == 0 || ws.ws_col == 0) {
        *rows = 24; *cols = 80;
    } else { *rows = ws.ws_row; *cols = ws.ws_col; }
}

static void draw_top_bar(void) {
    printf("\x1b[1;1H\x1b[2K\x1b[7m %s", filename ? filename : "[No Name]");
    if (sel_active) printf("  [SELECT]");
    printf("\x1b[K\x1b[27m");
}

static const char *resolve_style(int fi, int k,
                                 int m_start, int m_end,
                                 int current_match) {
    if (is_char_selected(fi, k)) return "\x1b[7m";     /* selection wins */
    for (int m = m_start; m < m_end; m++) {
        if (k >= matches[m].start && k < matches[m].start + match_len) {
            return (m == current_match) ? "\x1b[97;44m" : "\x1b[30;43m";
        }
    }
    return syntax_style_for_byte(fi, k);               /* tree-sitter last */
}

static void draw_body(void) {
    int vis_end = coloff + screen_cols;
    int m_cur   = find_first_match_on_line(rowoff);

    for (int i = 0; i < screen_rows; i++) {
        int fi = rowoff + i;
        if (fi >= nlines) break;
        Line *l = &lines[fi];
        if (l->len == 0) continue;

        while (m_cur < match_count && matches[m_cur].line < fi) m_cur++;
        int line_m_start = m_cur;
        while (m_cur < match_count && matches[m_cur].line == fi) m_cur++;
        int line_m_end = m_cur;

        printf("\x1b[%d;%dH", TEXT_Y0 + i + 1, TEXT_X0 + 1);

        int         vcol     = 0;
        const char *cur_ansi = NULL;

        for (int k = 0; k < l->len && vcol < vis_end; k++) {
            char        c    = l->chars[k];
            const char *want = resolve_style(fi, k, line_m_start, line_m_end, current_match);

            if (cur_ansi != want) {
                fputs("\x1b[0m", stdout);
                if (want) fputs(want, stdout);
                cur_ansi = want;
            }

            if (c == '\t') {
                int w   = TAB_WIDTH - (vcol % TAB_WIDTH);
                int end = vcol + w;
                int vs  = vcol > coloff ? vcol : coloff;
                int ve  = end  < vis_end ? end  : vis_end;
                for (int p = vs; p < ve; p++) putchar(' ');
                vcol = end;
            } else {
                if (vcol >= coloff) putchar(c);
                vcol++;
            }
        }
        if (cur_ansi) fputs("\x1b[0m", stdout);
    }
}

void editor_refresh(void) {
    int rows, cols;
    get_window_size(&rows, &cols);
    screen_rows = rows - TEXT_Y0 - MARGIN_BOTTOM - PADDING_BOTTOM - 1;
    screen_cols = cols - TEXT_X0 - MARGIN_RIGHT  - PADDING_RIGHT;
    if (screen_rows < 1) screen_rows = 1;
    if (screen_cols < 1) screen_cols = 1;

    scroll();
    syntax_ensure_parsed();

    fputs("\x1b[2J\x1b[H", stdout);
    draw_top_bar();
    draw_body();

    printf("\x1b[%d;1H\x1b[2K", rows);
    if (prompt_mode != PROMPT_NONE) {
        printf("%s%s", prompt_text, prompt_buf);
    } else if (search_active) {
        printf("Search: %s", search_buf);
        if (search_len > 0) {
            if (match_count == 0)             printf("  [no matches]");
            else if (matches_capped)          printf("  [%d/%d, capped]", current_match + 1, match_count);
            else                              printf("  [%d/%d]", current_match + 1, match_count);
        }
    } else {
        char help[512];
        if (status_msg[0])
            snprintf(help, sizeof(help), " %s", status_msg);
        else
            snprintf(help, sizeof(help),
                " Ctrl+S Save  Ctrl+O Open  Ctrl+F Find  Ctrl+A All  Ctrl+V Select  "
                "Ctrl+Z Undo  Ctrl+Y Redo  Ctrl+Q Quit");
        if ((int)strlen(help) > cols) help[cols] = '\0';
        fputs(help, stdout);
    }

    if (prompt_mode != PROMPT_NONE) {
        int px = 1 + (int)strlen(prompt_text) + prompt_len;
        if (px > cols) px = cols;
        printf("\x1b[%d;%dH", rows, px);
    } else if (search_active) {
        int px = 1 + (int)strlen("Search: ") + search_len;
        if (px > cols) px = cols;
        printf("\x1b[%d;%dH", rows, px);
    } else {
        int vcol  = visual_col_at(&lines[cy], cx);
        int scr_y = TEXT_Y0 + (cy - rowoff);
        int scr_x = TEXT_X0 + (vcol - coloff);
        if (scr_y < TEXT_Y0)                scr_y = TEXT_Y0;
        if (scr_y >= TEXT_Y0 + screen_rows) scr_y = TEXT_Y0 + screen_rows - 1;
        if (scr_x < TEXT_X0)                scr_x = TEXT_X0;
        if (scr_x >= TEXT_X0 + screen_cols) scr_x = TEXT_X0 + screen_cols - 1;
        printf("\x1b[%d;%dH", scr_y + 1, scr_x + 1);
    }
    fflush(stdout);
}