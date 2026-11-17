#include "ted.h"

PromptMode  prompt_mode = PROMPT_NONE;
const char *prompt_text = "";
char        prompt_buf[1024];
int         prompt_len = 0;

void prompt_begin(PromptMode mode, const char *text, const char *initial) {
    prompt_mode = mode; prompt_text = text; prompt_len = 0;
    if (initial) {
        int n = (int)strlen(initial);
        if (n > (int)sizeof(prompt_buf) - 1) n = (int)sizeof(prompt_buf) - 1;
        memcpy(prompt_buf, initial, (size_t)n);
        prompt_len = n;
    }
    prompt_buf[prompt_len] = '\0';
}

void prompt_confirm(void) {
    if (prompt_len == 0) { set_status("Empty input"); prompt_mode = PROMPT_NONE; return; }
    if (prompt_mode == PROMPT_SAVE) {
        if (editor_save(prompt_buf) == 0) { set_filename(prompt_buf); set_status("Saved"); }
        else                              set_status("Save failed");
    } else if (prompt_mode == PROMPT_OPEN) {
        if (editor_load(prompt_buf) == 0) { set_filename(prompt_buf); set_status("Opened"); }
        else                              set_status("Cannot open file");
    }
    prompt_mode = PROMPT_NONE;
}

void prompt_handle_key(int c) {
    if (c == KEY_ESC)   { prompt_mode = PROMPT_NONE; return; }
    if (c == KEY_ENTER) { prompt_confirm(); return; }
    if (c == KEY_BACKSPACE || c == CTRL_KEY('h')) {
        if (prompt_len > 0) prompt_buf[--prompt_len] = '\0';
        return;
    }
    if (c >= 32 && c < 127 && prompt_len < (int)sizeof(prompt_buf) - 1) {
        prompt_buf[prompt_len++] = (char)c;
        prompt_buf[prompt_len]   = '\0';
    }
}