#include "ted.h"
#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <unistd.h>
#include <sys/select.h>
#include <sys/wait.h>

char  *clipboard = NULL;
size_t clipboard_len = 0;

/* ---------- system clipboard ---------- */
static int tool_on_path(const char *name) {
    const char *path = getenv("PATH");
    if (!path) return 0;
    char buf[4096];
    while (*path) {
        const char *end = strchr(path, ':');
        size_t len = end ? (size_t)(end - path) : strlen(path);
        int n = snprintf(buf, sizeof(buf), "%.*s/%s", (int)len, path, name);
        if (n > 0 && (size_t)n < sizeof(buf) && access(buf, X_OK) == 0) return 1;
        if (!end) break;
        path = end + 1;
    }
    return 0;
}

static void exec_with_data(const char *const *argv, const char *data, size_t len) {
    int p[2];
    if (pipe(p) == -1) return;
    pid_t pid = fork();
    if (pid == -1) { close(p[0]); close(p[1]); return; }
    if (pid == 0) {
        close(p[1]);
        dup2(p[0], 0); close(p[0]);
        int dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 1); dup2(dn, 2); close(dn); }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(p[0]);
    size_t off = 0;
    while (off < len) {
        ssize_t n = write(p[1], data + off, len - off);
        if (n < 0) { if (errno == EINTR) continue; break; }
        off += (size_t)n;
    }
    close(p[1]);
}

int copy_to_system_clipboard(const char *data, size_t len) {
    static const char *const wl_copy[]    = { "wl-copy", NULL };
    static const char *const xclip_copy[] = { "xclip", "-selection", "clipboard", NULL };
    static const char *const xsel_copy[]  = { "xsel", "--clipboard", "--input", NULL };

    int on_wayland = getenv("WAYLAND_DISPLAY") != NULL ||
                     getenv("WAYLAND_SOCKET")  != NULL;
    int on_x11     = getenv("DISPLAY") != NULL;

    if (on_wayland && tool_on_path("wl-copy")) { exec_with_data(wl_copy,    data, len); return 0; }
    if (on_x11     && tool_on_path("xclip"))   { exec_with_data(xclip_copy, data, len); return 0; }
    if (on_x11     && tool_on_path("xsel"))    { exec_with_data(xsel_copy,  data, len); return 0; }
    return -1;
}

static char *run_paste_tool(const char *const *argv, size_t *out_len) {
    int p[2];
    if (pipe(p) == -1) return NULL;
    pid_t pid = fork();
    if (pid == -1) { close(p[0]); close(p[1]); return NULL; }
    if (pid == 0) {
        close(p[0]);
        dup2(p[1], 1); close(p[1]);
        int dn = open("/dev/null", O_RDONLY);
        if (dn >= 0) { dup2(dn, 0); close(dn); }
        dn = open("/dev/null", O_WRONLY);
        if (dn >= 0) { dup2(dn, 2); close(dn); }
        execvp(argv[0], (char *const *)argv);
        _exit(127);
    }
    close(p[1]);

    size_t cap = 256, len = 0;
    char  *buf = malloc(cap);
    int    hung = 0;
    if (!buf) { close(p[0]); return NULL; }

    for (;;) {
        fd_set fds; struct timeval tv = { 1, 0 };
        FD_ZERO(&fds); FD_SET(p[0], &fds);
        if (select(p[0] + 1, &fds, NULL, NULL, &tv) <= 0) { hung = 1; break; }
        if (len + 1 >= cap) {
            cap *= 2;
            char *t = realloc(buf, cap);
            if (!t) { free(buf); close(p[0]); return NULL; }
            buf = t;
        }
        ssize_t n = read(p[0], buf + len, cap - len - 1);
        if (n < 0) { if (errno == EINTR) continue; hung = 1; break; }
        if (n == 0) break;
        len += (size_t)n;
    }
    close(p[0]);

    if (hung) { kill(pid, SIGKILL); free(buf); return NULL; }
    for (int i = 0; i < 20; i++) {
        int status;
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) {
            if (!WIFEXITED(status) || WEXITSTATUS(status) != 0) {
                free(buf); return NULL;
            }
            break;
        }
        usleep(10000);
    }
    if (len == 0) { free(buf); return NULL; }
    buf[len] = '\0';
    *out_len = len;
    return buf;
}

char *paste_from_system_clipboard(size_t *out_len) {
    static const char *const wl_paste[]    = { "wl-paste", "--no-newline", NULL };
    static const char *const xclip_paste[] = { "xclip", "-selection", "clipboard", "-o", NULL };
    static const char *const xsel_paste[]  = { "xsel", "--clipboard", "--output", NULL };

    char *r;
    if (tool_on_path("wl-paste") && (r = run_paste_tool(wl_paste,    out_len))) return r;
    if (tool_on_path("xclip")    && (r = run_paste_tool(xclip_paste, out_len))) return r;
    if (tool_on_path("xsel")     && (r = run_paste_tool(xsel_paste,  out_len))) return r;
    return NULL;
}

/* ---------- internal clipboard ---------- */
static void editor_build_clipboard_from_selection(void) {
    if (!sel_active) return;
    int scy, scx, ecy, ecx;
    sel_get_bounds(&scy, &scx, &ecy, &ecx);

    size_t cap = 128, len = 0;
    char  *buf = malloc(cap);
    if (!buf) die("malloc");

    #define CLIP_APPEND(ch) do { \
        if (len + 1 >= cap) { cap *= 2; buf = realloc(buf, cap); if (!buf) die("realloc"); } \
        buf[len++] = (ch); \
    } while (0)

    if (scy == ecy) {
        Line *l = &lines[scy];
        for (int i = scx; i < ecx; i++) CLIP_APPEND(l->chars[i]);
    } else {
        Line *first = &lines[scy];
        for (int i = scx; i < first->len; i++) CLIP_APPEND(first->chars[i]);
        for (int row = scy + 1; row < ecy; row++) {
            CLIP_APPEND('\n');
            Line *mid = &lines[row];
            for (int i = 0; i < mid->len; i++) CLIP_APPEND(mid->chars[i]);
        }
        CLIP_APPEND('\n');
        Line *last = &lines[ecy];
        for (int i = 0; i < ecx; i++) CLIP_APPEND(last->chars[i]);
    }
    #undef CLIP_APPEND

    free(clipboard);
    clipboard = buf;
    clipboard_len = len;
}

void editor_yank_selection(void) {
    if (!sel_active) return;
    editor_build_clipboard_from_selection();
    sel_active = 0;
    preferred_col = -1;
    if (!clipboard || clipboard_len == 0) { set_status("Empty selection"); return; }

    if (copy_to_system_clipboard(clipboard, clipboard_len) == 0)
        set_status("Copied (system + internal)");
    else
        set_status("Copied (internal only  no wl-copy/xclip/xsel)");
}

void editor_paste(void) {
    if (!clipboard || clipboard_len == 0) return;
    undo_push();
    int was = undo_suppressed;
    undo_suppressed = 1;
    if (sel_active) editor_delete_selection();
    for (size_t i = 0; i < clipboard_len; i++) {
        if (clipboard[i] == '\n') editor_newline();
        else                      editor_insert_char(clipboard[i]);
    }
    undo_suppressed = was;
}

void editor_copy_selection_to_system(void) {
    if (!sel_active) { set_status("No selection"); return; }
    editor_build_clipboard_from_selection();
    if (clipboard && clipboard_len > 0) {
        if (copy_to_system_clipboard(clipboard, clipboard_len) == 0)
            set_status("Copied to system clipboard");
        else
            set_status("System clipboard unavailable (wl-copy/xclip/xsel)");
    }
    preferred_col = -1;
}

void editor_paste_from_system(void) {
    size_t len = 0;
    char  *text = paste_from_system_clipboard(&len);
    if (text) {
        free(clipboard);
        clipboard = text;
        clipboard_len = len;
    } else if (!clipboard || clipboard_len == 0) {
        set_status("Nothing to paste");
        return;
    }
    editor_paste();
}

void editor_insert_paste_buffer(const char *buf, size_t len) {
    if (len == 0) return;
    undo_push();
    int was = undo_suppressed;
    undo_suppressed = 1;
    if (sel_active) editor_delete_selection();
    for (size_t i = 0; i < len; i++) {
        unsigned char b = (unsigned char)buf[i];
        if (b == '\r') {
            if (i + 1 < len && buf[i + 1] == '\n') i++;
            editor_newline();
        } else if (b == '\n') {
            editor_newline();
        } else if (b == '\t') {
            editor_insert_char('\t');
        } else if (b >= 32 && b < 127) {
            editor_insert_char((char)b);
        }
    }
    undo_suppressed = was;
}