#include "ted.h"

char *filename = NULL;

void set_filename(const char *path) {
    free(filename);
    filename = path ? xstrdup(path) : NULL;
}

int editor_load(const char *path) {
    FILE *fp = fopen(path, "r");
    if (!fp) return -1;
    for (int i = 0; i < nlines; i++) free(lines[i].chars);
    nlines = 0;
    char *buf = NULL; size_t cap = 0; ssize_t n;
    while ((n = getline(&buf, &cap, fp)) != -1) {
        while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == '\r')) buf[--n] = '\0';
        Line l; line_init(&l);
        line_ensure(&l, (int)n + 1);
        if (n > 0) memcpy(l.chars, buf, (size_t)n);
        l.len = (int)n;
        insert_line_at(nlines, l);
    }
    free(buf); fclose(fp);
    if (nlines == 0) { Line l; line_init(&l); insert_line_at(0, l); }
    cy = cx = 0;
    rowoff = coloff = 0;
    preferred_col = -1;
    sel_active = 0;
    undo_clear();
    syntax_mark_dirty();    
    return 0;
}

int editor_save(const char *path) {
    FILE *fp = fopen(path, "w");
    if (!fp) return -1;
    for (int i = 0; i < nlines; i++) {
        fwrite(lines[i].chars, 1, (size_t)lines[i].len, fp);
        if (i < nlines - 1) fputc('\n', fp);
    }
    fclose(fp);
    return 0;
}