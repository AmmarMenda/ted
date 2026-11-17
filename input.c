#include "ted.h"
#include <unistd.h>
#include <sys/select.h>

char  *paste_buf = NULL;
size_t paste_len = 0;

#define IS_CTRL_SHIFT(mods) ((((mods) - 1) & 5) == 5)

static int read_escape_seq(void) {
    char seq[24]; int len = 0; int got_intro = 0;
    while (len < 22) {
        fd_set fds; struct timeval tv = { 0, 50000 };
        FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) break;
        char b;
        if (read(STDIN_FILENO, &b, 1) != 1) break;
        seq[len++] = b;
        if (!got_intro) {
            got_intro = 1;
            if (b != '[' && b != 'O') break;
            continue;
        }
        if (b >= 0x40 && b <= 0x7E) break;
    }
    seq[len] = '\0';
    if (len == 0) return KEY_ESC;

    char intro = seq[0];
    if (intro == 'O' && len >= 2) {
        switch (seq[1]) {
            case 'A': return KEY_UP;
            case 'B': return KEY_DOWN;
            case 'C': return KEY_RIGHT;
            case 'D': return KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
        }
        return KEY_NONE;
    }
    if (intro == '[') {
        if (len < 2) return KEY_NONE;
        char final = seq[len - 1];
        char params[24]; int plen = 0;
        for (int i = 1; i < len - 1 && plen < 23; i++) params[plen++] = seq[i];
        params[plen] = '\0';

        if (final == 'u') {
            int keycode = 0, mods = 0;
            if (sscanf(params, "%d;%d", &keycode, &mods) == 2 && IS_CTRL_SHIFT(mods)) {
                if (keycode == 'c' || keycode == 'C') return KEY_CTRL_SHIFT_C;
                if (keycode == 'v' || keycode == 'V') return KEY_CTRL_SHIFT_V;
            }
            return KEY_NONE;
        }
        if (final == '~') {
            if (!strcmp(params, "200")) return KEY_PASTE;
            if (!strcmp(params, "2;5")) return KEY_CTRL_INSERT;
            int a = 0, b = 0, c = 0;
            if (sscanf(params, "%d;%d;%d", &a, &b, &c) == 3 && a == 27 &&
                IS_CTRL_SHIFT(b)) {
                if (c == 'c' || c == 'C') return KEY_CTRL_SHIFT_C;
                if (c == 'v' || c == 'V') return KEY_CTRL_SHIFT_V;
            }
            if (!strcmp(params, "1") || !strcmp(params, "7")) return KEY_HOME;
            if (!strcmp(params, "4") || !strcmp(params, "8")) return KEY_END;
            if (!strcmp(params, "3")) return KEY_DEL;
            if (!strcmp(params, "5")) return KEY_PGUP;
            if (!strcmp(params, "6")) return KEY_PGDN;
            return KEY_NONE;
        }
        int ctrl = (strstr(params, ";5") != NULL);
        switch (final) {
            case 'A': return ctrl ? KEY_CTRL_UP    : KEY_UP;
            case 'B': return ctrl ? KEY_CTRL_DOWN  : KEY_DOWN;
            case 'C': return ctrl ? KEY_CTRL_RIGHT : KEY_RIGHT;
            case 'D': return ctrl ? KEY_CTRL_LEFT  : KEY_LEFT;
            case 'H': return KEY_HOME;
            case 'F': return KEY_END;
            case 'Z': return KEY_SHIFT_TAB;
        }
        return KEY_NONE;
    }
    return KEY_ESC;
}

void consume_paste_body(void) {
    free(paste_buf);
    paste_buf = NULL;
    paste_len = 0;

    size_t cap = 1024, len = 0;
    char  *buf = malloc(cap);
    if (!buf) return;

    for (;;) {
        fd_set fds; struct timeval tv = { 1, 0 };
        FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
        if (select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) <= 0) break;

        if (len + 1 >= cap) {
            cap *= 2;
            char *t = realloc(buf, cap);
            if (!t) { free(buf); return; }
            buf = t;
        }
        ssize_t n = read(STDIN_FILENO, buf + len, 1);
        if (n != 1) break;
        len++;
        if (len >= 6 && memcmp(buf + len - 6, "\x1b[201~", 6) == 0) {
            len -= 6;
            break;
        }
    }
    paste_buf = buf;
    paste_len = len;
}

int read_key(void) {
    char c; ssize_t n;
    while ((n = read(STDIN_FILENO, &c, 1)) == -1) { }
    if (n == 0) return KEY_NONE;
    if ((unsigned char)c == 0x1b) {
        int key = read_escape_seq();
        if (key == KEY_PASTE) { consume_paste_body(); return KEY_PASTE; }
        return key;
    }
    return (unsigned char)c;
}

int input_pending(void) {
    fd_set fds; struct timeval tv = { 0, 0 };
    FD_ZERO(&fds); FD_SET(STDIN_FILENO, &fds);
    return select(STDIN_FILENO + 1, &fds, NULL, NULL, &tv) > 0;
}