#include "syntax.h"
#include "ted.h"
#include <tree_sitter/api.h>

/* Provided by deps/tree-sitter-c/src/parser.c */
const TSLanguage *tree_sitter_c(void);

/* Minimal C grammar queries. Add more node types as needed. */
static const char *HIGHLIGHTS_SCM =
    "(comment) @comment\n"
    "(string_literal) @string\n"
    "(char_literal) @string\n"
    "(system_lib_string) @string\n"
    "(number_literal) @number\n"
    "[\n"
    "  \"#define\"\n"
    "  \"#elif\"\n"
    "  \"#elifdef\"\n"
    "  \"#elifndef\"\n"
    "  \"#else\"\n"
    "  \"#endif\"\n"
    "  \"#if\"\n"
    "  \"#ifdef\"\n"
    "  \"#ifndef\"\n"
    "  \"#include\"\n"
    "  \"defined\"\n"
    "] @preproc\n"
    "(preproc_directive) @preproc\n"
    "[\n"
    "  \"if\" \"else\" \"for\" \"while\" \"do\" \"switch\" \"case\" \"default\"\n"
    "  \"break\" \"continue\" \"return\" \"goto\" \"sizeof\" \"typedef\"\n"
    "  \"struct\" \"union\" \"enum\" \"static\" \"extern\" \"const\" \"volatile\"\n"
    "  \"register\" \"auto\" \"inline\" \"restrict\" \"_Atomic\" \"_Generic\"\n"
    "  \"_Alignas\" \"_Alignof\" \"_Noreturn\"\n"
    "  \"alignas\" \"alignof\" \"constexpr\" \"noreturn\" \"thread_local\"\n"
    "] @keyword\n"
    "[\n"
    "  (null)\n"
    "  (true)\n"
    "  (false)\n"
    "] @constant\n"
    "(primitive_type) @type\n"
    "(sized_type_specifier) @type\n"
    "(type_identifier) @type\n"
    "(macro_type_specifier) @type\n"
    "(field_identifier) @property\n"
    "(call_expression function: (identifier) @function)\n"
    "(call_expression function: (field_expression field: (field_identifier) @function))\n"
    "(function_declarator declarator: (identifier) @function)\n"
    "(preproc_function_def name: (identifier) @function)\n";

static const char *ansi_for_capture(const char *name, int len) {
    #define MATCH(s) (len == (int)sizeof(s)-1 && strncmp(name, s, (size_t)len) == 0)
    if (MATCH("comment"))  return "\x1b[90m";   /* bright black (gray) */
    if (MATCH("string"))   return "\x1b[32m";   /* green               */
    if (MATCH("number"))   return "\x1b[33m";   /* yellow              */
    if (MATCH("keyword"))  return "\x1b[35m";   /* magenta             */
    if (MATCH("constant")) return "\x1b[35m";   /* magenta             */
    if (MATCH("type"))     return "\x1b[36m";   /* cyan                */
    if (MATCH("function")) return "\x1b[34m";   /* blue                */
    if (MATCH("property")) return "\x1b[33m";   /* yellow              */
    if (MATCH("preproc"))  return "\x1b[95m";   /* bright magenta      */
    #undef MATCH
    return NULL;
}

/* ---------- static state ---------- */
static TSParser      *s_parser  = NULL;
static TSTree        *s_tree    = NULL;
static TSQuery       *s_query   = NULL;
static TSQueryCursor *s_cursor  = NULL;

static int *line_offsets     = NULL;   /* byte offset of each line in flattened buffer */
static int  line_offsets_cap = 0;

/* Per-visible-byte colour map (rebuilt every query run). */
static unsigned char  *byte_colored  = NULL;
static const char    **byte_ansi     = NULL;
static size_t          byte_map_cap  = 0;
static uint32_t        visible_start = 0;
static uint32_t        visible_end   = 0;
static size_t          visible_len   = 0;

static int buffer_dirty = 1;

/* ---------- helpers ---------- */

static char *flatten_buffer(size_t *out_len) {
    size_t total = 0;
    for (int i = 0; i < nlines; i++) total += (size_t)lines[i].len + 1;

    if (line_offsets_cap < nlines) {
        int *t = realloc(line_offsets, sizeof(int) * (size_t)nlines);
        if (!t) { *out_len = 0; return NULL; }
        line_offsets = t;
        line_offsets_cap = nlines;
    }

    char *buf = malloc(total + 1);
    if (!buf) { *out_len = 0; return NULL; }

    size_t off = 0;
    for (int i = 0; i < nlines; i++) {
        line_offsets[i] = (int)off;
        if (lines[i].len > 0) {
            memcpy(buf + off, lines[i].chars, (size_t)lines[i].len);
            off += (size_t)lines[i].len;
        }
        if (i < nlines - 1) buf[off++] = '\n';
    }
    buf[off] = '\0';
    *out_len = off;
    return buf;
}

/* ---------- public API ---------- */

void syntax_init(void) {
    s_parser = ts_parser_new();
    if (!s_parser) return;
    ts_parser_set_language(s_parser, tree_sitter_c());

    TSQueryError err;
    uint32_t     err_off;
    s_query = ts_query_new(tree_sitter_c(),
                           HIGHLIGHTS_SCM,
                           (uint32_t)strlen(HIGHLIGHTS_SCM),
                           &err_off, &err);
    if (!s_query) {
        fprintf(stderr, "ted: tree-sitter query failed at offset %u (err %d)\n",
                err_off, (int)err);
    }
    s_cursor = ts_query_cursor_new();
    buffer_dirty = 1;
}

void syntax_shutdown(void) {
    if (s_tree)   ts_tree_delete(s_tree);
    if (s_parser) ts_parser_delete(s_parser);
    if (s_query)  ts_query_delete(s_query);
    if (s_cursor) ts_query_cursor_delete(s_cursor);
    free(line_offsets);
    free(byte_colored);
    free(byte_ansi);
    s_tree = NULL; s_parser = NULL; s_query = NULL; s_cursor = NULL;
    line_offsets = NULL; byte_colored = NULL; byte_ansi = NULL;
    line_offsets_cap = 0; byte_map_cap = 0;
    visible_start = visible_end = 0; visible_len = 0;
}

void syntax_mark_dirty(void) { buffer_dirty = 1; }

void syntax_ensure_parsed(void) {
    if (!s_parser) return;

    /* 1. (Re)parse if the buffer changed since the last render. */
    if (buffer_dirty) {
        buffer_dirty = 0;
        size_t len;
        char *src = flatten_buffer(&len);
        if (!src) return;
        TSTree *new_tree = ts_parser_parse_string(s_parser, NULL, src, (uint32_t)len);
        if (s_tree) ts_tree_delete(s_tree);
        s_tree = new_tree;
        free(src);
    }
    if (!s_tree || !s_query || !s_cursor) return;

    /* 2. Compute the visible byte range. */
    if (!line_offsets || nlines <= 0) return;
    int top_line = rowoff;
    int bot_line = rowoff + screen_rows;
    if (bot_line > nlines) bot_line = nlines;
    if (top_line >= nlines) {
        visible_start = visible_end = 0;
        visible_len = 0;
        return;
    }
    if (top_line < 0) top_line = 0;

    visible_start = (uint32_t)line_offsets[top_line];
    if (bot_line < nlines) {
        visible_end = (uint32_t)line_offsets[bot_line];
    } else {
        visible_end = (uint32_t)(line_offsets[nlines - 1] + lines[nlines - 1].len);
    }
    if (visible_end < visible_start) visible_end = visible_start;
    visible_len = (size_t)(visible_end - visible_start);

    /* 3. Allocate / reset the per-byte colour map. */
    if (visible_len == 0) {
        visible_start = visible_end = 0;
        return;
    }
    if (byte_map_cap < visible_len) {
        unsigned char *nc = realloc(byte_colored, visible_len);
        const char   **na = realloc(byte_ansi, visible_len * sizeof(*na));
        if (!nc || !na) {
            free(nc); free(na);
            byte_colored = NULL; byte_ansi = NULL; byte_map_cap = 0;
            visible_start = visible_end = 0;
            visible_len = 0;
            return;
        }
        byte_colored = nc;
        byte_ansi    = na;
        byte_map_cap = visible_len;
    }
    memset(byte_colored, 0, visible_len);
    memset(byte_ansi,    0, visible_len * sizeof(*byte_ansi));

    /* 4. Run the query limited to the visible byte range. */
    ts_query_cursor_set_byte_range(s_cursor, visible_start, visible_end);
    ts_query_cursor_exec(s_cursor, s_query, ts_tree_root_node(s_tree));

    TSQueryMatch m;
    uint32_t cap_idx;
    while (ts_query_cursor_next_capture(s_cursor, &m, &cap_idx)) {
        TSNode node = m.captures[cap_idx].node;
        uint32_t name_len;
        const char *name = ts_query_capture_name_for_id(s_query, m.captures[cap_idx].index, &name_len);
        const char *ansi = ansi_for_capture(name, (int)name_len);
        if (!ansi) continue;

        uint32_t s = ts_node_start_byte(node);
        uint32_t e = ts_node_end_byte(node);
        if (s < visible_start) s = visible_start;
        if (e > visible_end)   e = visible_end;
        if (s >= e) continue;

        /* Last wins: later captures overwrite earlier ones. */
        for (uint32_t b = s; b < e; b++) {
            size_t rel = (size_t)(b - visible_start);
            byte_colored[rel] = 1;
            byte_ansi[rel]    = ansi;
        }
    }
}

const char *syntax_style_for_byte(int line_idx, int byte_idx) {
    if (line_idx < 0 || line_idx >= nlines) return NULL;
    if (!line_offsets || line_idx >= line_offsets_cap) return NULL;
    if (byte_idx < 0 || byte_idx >= lines[line_idx].len) return NULL;
    uint32_t abs_off = (uint32_t)(line_offsets[line_idx] + byte_idx);
    if (abs_off < visible_start || abs_off >= visible_end) return NULL;
    if (!byte_colored || !byte_ansi) return NULL;
    size_t rel = (size_t)(abs_off - visible_start);
    if (rel >= visible_len || !byte_colored[rel]) return NULL;
    return byte_ansi[rel];
}