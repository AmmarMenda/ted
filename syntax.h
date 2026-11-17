#ifndef SYNTAX_H
#define SYNTAX_H

void        syntax_init(void);
void        syntax_shutdown(void);
void        syntax_mark_dirty(void);          /* call after any buffer mutation */
void        syntax_ensure_parsed(void);       /* call at top of each render      */
const char *syntax_style_for_byte(int line_idx, int byte_idx);  /* ANSI SGR or NULL */

#endif