#include "ted.h"

typedef struct {
    Line *lines;
    int   nlines;
    int   cy, cx;
} Snapshot;

static Snapshot *undo_stack = NULL;
static int undo_count = 0, undo_cap = 0;
static Snapshot *redo_stack = NULL;
static int redo_count = 0, redo_cap = 0;
int undo_suppressed = 0;

#define UNDO_LIMIT 500

static Snapshot snapshot_capture(void) {
    Snapshot s;
    s.nlines = nlines;
    s.cy = cy;
    s.cx = cx;
    s.lines = malloc(sizeof(Line) * (size_t)(nlines ? nlines : 1));
    if (!s.lines) die("malloc");
    for (int i = 0; i < nlines; i++) {
        s.lines[i].len = lines[i].len;
        s.lines[i].cap = lines[i].len > 0 ? lines[i].len : 1;
        s.lines[i].chars = malloc((size_t)s.lines[i].cap);
        if (!s.lines[i].chars) die("malloc");
        if (lines[i].len > 0)
            memcpy(s.lines[i].chars, lines[i].chars, (size_t)lines[i].len);
    }
    return s;
}
static void snapshot_free(Snapshot *s) {
    for (int i = 0; i < s->nlines; i++) free(s->lines[i].chars);
    free(s->lines);
    s->lines = NULL;
    s->nlines = 0;
}
static void snapshot_restore(Snapshot *s) {
    for (int i = 0; i < nlines; i++) free(lines[i].chars);
    free(lines);
    lines     = s->lines;
    nlines    = s->nlines;
    cap_lines = nlines > 0 ? nlines : 1;
    cy = s->cy;
    cx = s->cx;
    s->lines  = NULL;
    s->nlines = 0;
    if (nlines == 0) {
        Line l; line_init(&l);
        lines[0] = l;
        nlines = 1;
        cy = cx = 0;
    }
    if (cy >= nlines) cy = nlines - 1;
    if (cx > lines[cy].len) cx = lines[cy].len;
    preferred_col = -1;
    sel_active = 0;
    syntax_mark_dirty();
}
void undo_clear(void) {
    for (int i = 0; i < undo_count; i++) snapshot_free(&undo_stack[i]);
    undo_count = 0;
    for (int i = 0; i < redo_count; i++) snapshot_free(&redo_stack[i]);
    redo_count = 0;
}
void undo_clear_redo(void) {
    for (int i = 0; i < redo_count; i++) snapshot_free(&redo_stack[i]);
    redo_count = 0;
}
void undo_push(void) {
    if (undo_count >= UNDO_LIMIT) {
        snapshot_free(&undo_stack[0]);
        memmove(undo_stack, undo_stack + 1, (size_t)(undo_count - 1) * sizeof(Snapshot));
        undo_count--;
    }
    if (undo_count >= undo_cap) {
        undo_cap = undo_cap ? undo_cap * 2 : 32;
        undo_stack = realloc(undo_stack, (size_t)undo_cap * sizeof(Snapshot));
        if (!undo_stack) die("realloc");
    }
    undo_stack[undo_count++] = snapshot_capture();
    undo_clear_redo();
}
void undo_maybe(void) {
    if (!undo_suppressed) undo_push();
}
void editor_undo(void) {
    if (undo_count == 0) { set_status("Nothing to undo"); return; }
    if (redo_count >= redo_cap) {
        redo_cap = redo_cap ? redo_cap * 2 : 32;
        redo_stack = realloc(redo_stack, (size_t)redo_cap * sizeof(Snapshot));
        if (!redo_stack) die("realloc");
    }
    redo_stack[redo_count++] = snapshot_capture();
    Snapshot s = undo_stack[--undo_count];
    snapshot_restore(&s);
    set_status("Undo");
}
void editor_redo(void) {
    if (redo_count == 0) { set_status("Nothing to redo"); return; }
    if (undo_count >= undo_cap) {
        undo_cap = undo_cap ? undo_cap * 2 : 32;
        undo_stack = realloc(undo_stack, (size_t)undo_cap * sizeof(Snapshot));
        if (!undo_stack) die("realloc");
    }
    undo_stack[undo_count++] = snapshot_capture();
    Snapshot s = redo_stack[--redo_count];
    snapshot_restore(&s);
    set_status("Redo");
}