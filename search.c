#include "ted.h"
#include <ctype.h>

#define MAX_MATCHES 100000

Match *matches = NULL;
int    match_count = 0;
static int match_cap = 0;
int    match_len = 0;
int    matches_capped = 0;
int    current_match = -1;
int    search_active = 0;
char   search_buf[SEARCH_BUF_MAX];
int    search_len = 0;
static int search_saved_cy = 0;
static int search_saved_cx = 0;

void search_clear_matches(void) {
    free(matches);
    matches = NULL;
    match_count = 0;
    match_cap = 0;
    match_len = 0;
    matches_capped = 0;
    current_match = -1;
}

static int add_match(int line, int start) {
    if (match_count >= MAX_MATCHES) { matches_capped = 1; return 0; }
    if (match_count >= match_cap) {
        int newcap = match_cap ? match_cap * 2 : 64;
        if (newcap > MAX_MATCHES) newcap = MAX_MATCHES;
        Match *tmp = realloc(matches, sizeof(Match) * (size_t)newcap);
        if (!tmp) return 0;
        matches = tmp;
        match_cap = newcap;
    }
    matches[match_count].line  = line;
    matches[match_count].start = start;
    match_count++;
    return 1;
}

static void bmh_scan_line(int line_idx, const Line *l,
                          const unsigned char *needle, int nlen,
                          const int *skip) {
    if (nlen == 0 || l->len < nlen) return;
    const unsigned char *hay = (const unsigned char *)l->chars;
    int hlen = l->len;
    int pos  = 0;
    while (pos <= hlen - nlen) {
        int j = nlen - 1;
        while (j >= 0 &&
               (unsigned char)tolower(hay[pos + j]) == needle[j])
            j--;
        if (j < 0) {
            if (!add_match(line_idx, pos)) return;
            pos += 1;
        } else {
            pos += skip[hay[pos + j]];
        }
    }
}

static void run_search(void) {
    search_clear_matches();
    if (search_len == 0) return;

    unsigned char needle[SEARCH_BUF_MAX];
    for (int i = 0; i < search_len; i++)
        needle[i] = (unsigned char)tolower((unsigned char)search_buf[i]);
    needle[search_len] = 0;

    int skip[256];
    for (int i = 0; i < 256; i++) skip[i] = search_len;
    for (int i = 0; i < search_len - 1; i++) {
        unsigned char lo = needle[i];
        unsigned char up = (unsigned char)toupper(lo);
        skip[lo] = search_len - 1 - i;
        skip[up] = search_len - 1 - i;
    }

    match_len = search_len;
    for (int line = 0; line < nlines && !matches_capped; line++)
        bmh_scan_line(line, &lines[line], needle, search_len, skip);

    if (match_count > 0) {
        current_match = 0;
        cy = matches[0].line;
        cx = matches[0].start;
        if (cx > lines[cy].len) cx = lines[cy].len;
        preferred_col = -1;
    }
}

void jump_to_match(int idx) {
    if (idx < 0 || idx >= match_count) return;
    cy = matches[idx].line;
    cx = matches[idx].start;
    if (cx > lines[cy].len) cx = lines[cy].len;
    preferred_col = -1;
}

void search_begin(void) {
    search_active   = 1;
    search_len      = 0;
    search_buf[0]   = '\0';
    search_saved_cy = cy;
    search_saved_cx = cx;
    search_clear_matches();
    if (sel_active) sel_active = 0;
}

void search_exit(int restore) {
    search_active = 0;
    search_clear_matches();
    if (restore) {
        cy = search_saved_cy;
        cx = search_saved_cx;
        if (cy < 0) cy = 0;
        if (cy >= nlines) cy = nlines - 1;
        if (cx > lines[cy].len) cx = lines[cy].len;
        preferred_col = -1;
    }
}

void search_handle_key(int c) {
    if (c == KEY_ESC)       { search_exit(1); return; }
    if (c == KEY_ENTER)     { search_exit(0); return; }
    if (c == CTRL_KEY('f')) { search_exit(0); return; }
    if (c == KEY_TAB) {
        if (match_count > 0) {
            current_match = (current_match + 1) % match_count;
            jump_to_match(current_match);
        }
        return;
    }
    if (c == KEY_SHIFT_TAB) {
        if (match_count > 0) {
            current_match = (current_match - 1 + match_count) % match_count;
            jump_to_match(current_match);
        }
        return;
    }
    if (c == KEY_BACKSPACE || c == CTRL_KEY('h')) {
        if (search_len > 0) {
            search_buf[--search_len] = '\0';
            run_search();
        }
        return;
    }
    if (c >= 32 && c < 127 && search_len < SEARCH_BUF_MAX - 1) {
        search_buf[search_len++] = (char)c;
        search_buf[search_len]   = '\0';
        run_search();
    }
}

int find_first_match_on_line(int line) {
    int lo = 0, hi = match_count - 1, res = match_count;
    while (lo <= hi) {
        int mid = (lo + hi) / 2;
        if (matches[mid].line >= line) { res = mid; hi = mid - 1; }
        else lo = mid + 1;
    }
    return res;
}