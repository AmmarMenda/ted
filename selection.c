#include "ted.h"

int sel_active = 0;
int sel_anchor_cy = 0, sel_anchor_cx = 0;

void sel_get_bounds(int *scy, int *scx, int *ecy, int *ecx) {
    int anchor_before =
        (sel_anchor_cy < cy) || (sel_anchor_cy == cy && sel_anchor_cx <= cx);
    if (anchor_before) {
        *scy = sel_anchor_cy; *scx = sel_anchor_cx;
        *ecy = cy;            *ecx = cx;
    } else {
        *scy = cy;            *scx = cx;
        *ecy = sel_anchor_cy; *ecx = sel_anchor_cx;
    }
}

int is_char_selected(int line_idx, int byte_idx) {
    if (!sel_active) return 0;
    int scy, scx, ecy, ecx;
    sel_get_bounds(&scy, &scx, &ecy, &ecx);
    if (line_idx < scy || line_idx > ecy) return 0;
    if (scy == ecy)          return byte_idx >= scx && byte_idx < ecx;
    if (line_idx == scy)     return byte_idx >= scx;
    if (line_idx == ecy)     return byte_idx < ecx;
    return 1;
}

void sel_select_all(void) {
    if (nlines == 0) return;
    sel_active    = 1;
    sel_anchor_cy = 0;
    sel_anchor_cx = 0;
    cy = nlines - 1;
    cx = lines[cy].len;
    preferred_col = -1;
}

void editor_delete_selection(void) {
    if (!sel_active) return;
    undo_maybe();
    int scy, scx, ecy, ecx;
    sel_get_bounds(&scy, &scx, &ecy, &ecx);
    if (scy == ecy) {
        Line *l = &lines[scy];
        int count = ecx - scx;
        if (count > 0) {
            memmove(l->chars + scx, l->chars + ecx, (size_t)(l->len - ecx));
            l->len -= count;
        }
    } else {
        Line *first = &lines[scy], *last = &lines[ecy];
        int   tail  = last->len - ecx;
        line_ensure(first, scx + tail);
        if (tail > 0) memmove(first->chars + scx, last->chars + ecx, (size_t)tail);
        first->len = scx + tail;
        for (int i = ecy; i > scy; i--) delete_line_at(i);
    }
    cy = scy; cx = scx;
    sel_active = 0;
    preferred_col = -1;
    syntax_mark_dirty();
}