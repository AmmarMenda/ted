#include "ted.h"

Line *lines = NULL;
int   nlines = 0, cap_lines = 0;
int   cy = 0, cx = 0;
int   rowoff = 0, coloff = 0;
int   preferred_col = -1;
int   screen_rows = 20, screen_cols = 80;

void line_init(Line *l) {
    l->cap = 16; l->len = 0;
    l->chars = malloc((size_t)l->cap);
    if (!l->chars) die("malloc");
}
void line_ensure(Line *l, int need) {
    while (l->cap < need) {
        l->cap *= 2;
        l->chars = realloc(l->chars, (size_t)l->cap);
        if (!l->chars) die("realloc");
    }
}
void line_insert_char(Line *l, int at, char c) {
    line_ensure(l, l->len + 1);
    memmove(l->chars + at + 1, l->chars + at, (size_t)(l->len - at));
    l->chars[at] = c; l->len++;
}
void line_delete_char(Line *l, int at) {
    if (at < 0 || at >= l->len) return;
    memmove(l->chars + at, l->chars + at + 1, (size_t)(l->len - at - 1));
    l->len--;
}
void insert_line_at(int at, Line nl) {
    if (nlines + 1 > cap_lines) {
        cap_lines = cap_lines ? cap_lines * 2 : 16;
        lines = realloc(lines, (size_t)cap_lines * sizeof(Line));
        if (!lines) die("realloc");
    }
    if (nlines - at > 0)
        memmove(&lines[at + 1], &lines[at], (size_t)(nlines - at) * sizeof(Line));
    lines[at] = nl; nlines++;
}
void delete_line_at(int at) {
    free(lines[at].chars);
    if (nlines - at - 1 > 0)
        memmove(&lines[at], &lines[at + 1],
                (size_t)(nlines - at - 1) * sizeof(Line));
    nlines--;
}

int visual_col_at(const Line *l, int cx) {
    if (cx < 0) cx = 0;
    if (cx > l->len) cx = l->len;
    int col = 0;
    for (int i = 0; i < cx; i++)
        col += (l->chars[i] == '\t') ? TAB_WIDTH - (col % TAB_WIDTH) : 1;
    return col;
}
int cx_for_visual_col(const Line *l, int target) {
    int vcol = 0;
    for (int i = 0; i < l->len; i++) {
        int w = (l->chars[i] == '\t') ? TAB_WIDTH - (vcol % TAB_WIDTH) : 1;
        if (vcol + w > target) return i;
        vcol += w;
    }
    return l->len;
}

void editor_insert_char(char c) {
    undo_maybe();
    line_insert_char(&lines[cy], cx, c);
    cx++;
    preferred_col = -1;
    syntax_mark_dirty();
}
void editor_newline(void) {
    undo_maybe();
    Line *cur = &lines[cy];
    Line  nl; line_init(&nl);
    int tail = cur->len - cx;
    line_ensure(&nl, tail);
    if (tail > 0) memcpy(nl.chars, cur->chars + cx, (size_t)tail);
    nl.len = tail; cur->len = cx;
    insert_line_at(cy + 1, nl);
    cy++; cx = 0;
    preferred_col = -1;
    syntax_mark_dirty();
}
void editor_backspace(void) {
    undo_maybe();
    if (cx > 0) { line_delete_char(&lines[cy], cx - 1); cx--; }
    else if (cy > 0) {
        Line *prev = &lines[cy - 1], *cur = &lines[cy];
        int   plen = prev->len;
        line_ensure(prev, plen + cur->len);
        if (cur->len > 0) memcpy(prev->chars + plen, cur->chars, (size_t)cur->len);
        prev->len = plen + cur->len;
        delete_line_at(cy);
        cy--; cx = plen;
    }
    preferred_col = -1;
    syntax_mark_dirty();
}
void editor_delete_forward(void) {
    undo_maybe();
    if (cx < lines[cy].len) line_delete_char(&lines[cy], cx);
    else if (cy < nlines - 1) {
        Line *cur = &lines[cy], *next = &lines[cy + 1];
        line_ensure(cur, cur->len + next->len);
        if (next->len > 0) memcpy(cur->chars + cur->len, next->chars, (size_t)next->len);
        cur->len += next->len;
        delete_line_at(cy + 1);
    }
    preferred_col = -1;
    syntax_mark_dirty();
}

void cursor_left(void) {
    if (cx > 0) cx--;
    else if (cy > 0) { cy--; cx = lines[cy].len; }
    preferred_col = -1;
}
void cursor_right(void) {
    if (cx < lines[cy].len) cx++;
    else if (cy < nlines - 1) { cy++; cx = 0; }
    preferred_col = -1;
}
void cursor_up(void) {
    if (cy == 0) return;
    int target = preferred_col >= 0 ? preferred_col : visual_col_at(&lines[cy], cx);
    cy--;
    cx = cx_for_visual_col(&lines[cy], target);
    preferred_col = target;
}
void cursor_down(void) {
    if (cy >= nlines - 1) return;
    int target = preferred_col >= 0 ? preferred_col : visual_col_at(&lines[cy], cx);
    cy++;
    cx = cx_for_visual_col(&lines[cy], target);
    preferred_col = target;
}
void cursor_home(void) { cx = 0; preferred_col = -1; }
void cursor_end(void)  { cx = lines[cy].len; preferred_col = -1; }
void cursor_top(void) {
    cy = 0; if (cx > lines[0].len) cx = lines[0].len;
    preferred_col = -1;
}
void cursor_bottom(void) {
    cy = nlines - 1; if (cx > lines[cy].len) cx = lines[cy].len;
    preferred_col = -1;
}
void cursor_page_up(void) {
    int step = screen_rows - 1; if (step < 1) step = 1;
    int target = preferred_col >= 0 ? preferred_col : visual_col_at(&lines[cy], cx);
    cy -= step; if (cy < 0) cy = 0;
    cx = cx_for_visual_col(&lines[cy], target);
    preferred_col = target;
}
void cursor_page_down(void) {
    int step = screen_rows - 1; if (step < 1) step = 1;
    int target = preferred_col >= 0 ? preferred_col : visual_col_at(&lines[cy], cx);
    cy += step; if (cy >= nlines) cy = nlines - 1;
    cx = cx_for_visual_col(&lines[cy], target);
    preferred_col = target;
}

void scroll(void) {
    if (cy < rowoff)                rowoff = cy;
    if (cy >= rowoff + screen_rows) rowoff = cy - screen_rows + 1;
    if (rowoff < 0)                 rowoff = 0;
    int vcol = visual_col_at(&lines[cy], cx);
    if (vcol < coloff)                coloff = vcol;
    if (vcol >= coloff + screen_cols) coloff = vcol - screen_cols + 1;
    if (coloff < 0)                   coloff = 0;
}