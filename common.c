#include "ted.h"

char status_msg[256];

void die(const char *msg) { perror(msg); exit(1); }

char *xstrdup(const char *s) {
    size_t n = strlen(s) + 1;
    char  *p = malloc(n);
    if (!p) die("malloc");
    memcpy(p, s, n);
    return p;
}

void set_status(const char *msg) {
    if (!msg) { status_msg[0] = '\0'; return; }
    snprintf(status_msg, sizeof(status_msg), "%s", msg);
}