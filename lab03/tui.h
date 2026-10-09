/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#ifndef LAB02_TUI_H
#define LAB02_TUI_H

#include <stdbool.h>
#include <stddef.h>

enum tui_key_kind {
    TUI_KEY_CHARACTER,
    TUI_KEY_UP,
    TUI_KEY_DOWN,
    TUI_KEY_LEFT,
    TUI_KEY_RIGHT,
    TUI_KEY_ENTER,
    TUI_KEY_ESCAPE,
    TUI_KEY_BACKSPACE,
};

struct tui_event {
    enum tui_key_kind kind;
    int character;
};

bool tui_init(void);
void tui_shutdown(void);

void tui_clear(void);
void tui_hide_cursor(void);
void tui_show_cursor(void);
void tui_printf(const char *format, ...);

struct tui_event tui_read_key(void);
bool tui_read_line(const char *prompt, char *buffer, size_t capacity);
void tui_wait(void);

#endif /* LAB02_TUI_H */
