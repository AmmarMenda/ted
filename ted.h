#ifndef TED_H
#define TED_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "syntax.h"
#define CTRL_KEY(k) ((k) & 0x1f)
#define TAB_WIDTH   8

/* ---------- geometry ---------- */
#define TOPBAR_HEIGHT  1
#define MARGIN_TOP     1
#define MARGIN_BOTTOM  1
#define MARGIN_LEFT    2
#define MARGIN_RIGHT   2
#define PADDING_TOP    1
#define PADDING_BOTTOM 1
#define PADDING_LEFT   1
#define PADDING_RIGHT  1
#define TEXT_X0 (MARGIN_LEFT + PADDING_LEFT)
#define TEXT_Y0 (TOPBAR_HEIGHT + MARGIN_TOP + PADDING_TOP)

/* ---------- keys ---------- */
enum {
    KEY_NONE = -1,
    KEY_ESC  = 27,
    KEY_ENTER = '\r',
    KEY_TAB   = '\t',
    KEY_BACKSPACE = 127,
    KEY_UP = 1000, KEY_DOWN, KEY_LEFT, KEY_RIGHT,
    KEY_CTRL_UP, KEY_CTRL_DOWN, KEY_CTRL_LEFT, KEY_CTRL_RIGHT,
    KEY_HOME, KEY_END, KEY_PGUP, KEY_PGDN, KEY_DEL,
    KEY_SHIFT_TAB,
    KEY_CTRL_SHIFT_C, KEY_CTRL_SHIFT_V,
    KEY_PASTE, KEY_CTRL_INSERT,
};

typedef struct { char *chars; int len; int cap; } Line;

typedef enum {
    STYLE_NORMAL = 0,
    STYLE_SEL,
    STYLE_MATCH,
    STYLE_CUR_MATCH,
} CharStyle;

typedef enum { PROMPT_NONE, PROMPT_SAVE, PROMPT_OPEN } PromptMode;

typedef struct { int line; int start; } Match;

/* ---------- shared state ---------- */
extern Line *lines;
extern int   nlines, cap_lines;
extern int   cy, cx;
extern int   rowoff, coloff;
extern int   preferred_col;
extern int   screen_rows, screen_cols;
extern char *filename;

extern int   sel_active;
extern int   sel_anchor_cy, sel_anchor_cx;

extern char  *clipboard;
extern size_t clipboard_len;

extern PromptMode  prompt_mode;
extern const char *prompt_text;
extern char        prompt_buf[1024];
extern int         prompt_len;
extern char        status_msg[256];

#define SEARCH_BUF_MAX 256
extern int    search_active;
extern char   search_buf[SEARCH_BUF_MAX];
extern int    search_len;
extern Match *matches;
extern int    match_count;
extern int    match_len;
extern int    current_match;
extern int    matches_capped;

extern char  *paste_buf;
extern size_t paste_len;

extern int undo_suppressed;

/* ---------- common.c ---------- */
void  die(const char *msg);
char *xstrdup(const char *s);
void  set_status(const char *msg);

/* ---------- buffer.c ---------- */
void line_init(Line *l);
void line_ensure(Line *l, int need);
void line_insert_char(Line *l, int at, char c);
void line_delete_char(Line *l, int at);
void insert_line_at(int at, Line nl);
void delete_line_at(int at);
int  visual_col_at(const Line *l, int cx);
int  cx_for_visual_col(const Line *l, int target);
void editor_insert_char(char c);
void editor_newline(void);
void editor_backspace(void);
void editor_delete_forward(void);
void cursor_left(void);
void cursor_right(void);
void cursor_up(void);
void cursor_down(void);
void cursor_home(void);
void cursor_end(void);
void cursor_top(void);
void cursor_bottom(void);
void cursor_page_up(void);
void cursor_page_down(void);
void scroll(void);

/* ---------- undo.c ---------- */
void undo_push(void);
void undo_maybe(void);
void editor_undo(void);
void editor_redo(void);
void undo_clear(void);
void undo_clear_redo(void);

/* ---------- file.c ---------- */
void set_filename(const char *path);
int  editor_load(const char *path);
int  editor_save(const char *path);

/* ---------- selection.c ---------- */
void sel_get_bounds(int *scy, int *scx, int *ecy, int *ecx);
int  is_char_selected(int line_idx, int byte_idx);
void sel_select_all(void);
void editor_delete_selection(void);

/* ---------- copypaste.c ---------- */
int   copy_to_system_clipboard(const char *data, size_t len);
char *paste_from_system_clipboard(size_t *out_len);
void  editor_yank_selection(void);
void  editor_paste(void);
void  editor_copy_selection_to_system(void);
void  editor_paste_from_system(void);
void  editor_insert_paste_buffer(const char *buf, size_t len);

/* ---------- prompt.c ---------- */
void prompt_begin(PromptMode mode, const char *text, const char *initial);
void prompt_confirm(void);
void prompt_handle_key(int c);

/* ---------- search.c ---------- */
void search_begin(void);
void search_exit(int restore);
void search_handle_key(int c);
void search_clear_matches(void);
int  find_first_match_on_line(int line);
void jump_to_match(int idx);

/* ---------- render.c ---------- */
void editor_refresh(void);

/* ---------- input.c ---------- */
int  read_key(void);
int  input_pending(void);
void consume_paste_body(void);

#endif