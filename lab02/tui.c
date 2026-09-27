/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#include "tui.h"

#include <poll.h>
#include <stdarg.h>
#include <stdio.h>
#include <termios.h>
#include <unistd.h>

struct tui_state {
    struct termios saved_termios;
    bool termios_saved;
    bool raw_mode;
    bool alternate_screen;
};

static struct tui_state tui_state;

static int tui_read_byte(void)
{
    unsigned char byte;
    ssize_t count = read(STDIN_FILENO, &byte, sizeof(byte));

    return count == (ssize_t)sizeof(byte) ? byte : -1;
}

bool tui_init(void)
{
    if (tcgetattr(STDIN_FILENO, &tui_state.saved_termios) < 0) {
        return false;
    }
    tui_state.termios_saved = true;

    struct termios raw = tui_state.saved_termios;
    cfmakeraw(&raw);

    raw.c_oflag |= OPOST | ONLCR;

    if (tcsetattr(STDIN_FILENO, TCSAFLUSH, &raw) < 0) {
        tui_state.termios_saved = false;
        return false;
    }
    tui_state.raw_mode = true;

    /* DEC private mode 1049: 切换到备用屏幕缓冲区 */
    fputs("\033[?1049h\033[H\033[?25l", stdout);
    fflush(stdout);
    tui_state.alternate_screen = true;
    return true;
}

void tui_shutdown(void)
{
    if (tui_state.alternate_screen) {
        tui_show_cursor();
        fputs("\033[?1049l", stdout);
        fflush(stdout);
        tui_state.alternate_screen = false;
    }

    if (tui_state.raw_mode && tui_state.termios_saved) {
        tcsetattr(STDIN_FILENO, TCSAFLUSH, &tui_state.saved_termios);
        tui_state.raw_mode = false;
    }
}

void tui_clear(void)
{
    fputs("\033[2J\033[H", stdout);
}

void tui_hide_cursor(void)
{
    fputs("\033[?25l", stdout);
}

void tui_show_cursor(void)
{
    fputs("\033[?25h", stdout);
}

void tui_printf(const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vprintf(format, arguments);
    va_end(arguments);
}

struct tui_event tui_read_key(void)
{
    int character = tui_read_byte();
    if (character < 0) {
        return (struct tui_event){ .kind = TUI_KEY_ESCAPE };
    }

    if (character == '\033') {
        struct pollfd input = {
            .fd = STDIN_FILENO,
            .events = POLLIN,
        };
        if (poll(&input, 1, 25) <= 0) {
            return (struct tui_event){ .kind = TUI_KEY_ESCAPE };
        }

        int sequence = tui_read_byte();
        if (sequence == '[') {
            int code = tui_read_byte();
            switch (code) {
            case 'A':
                return (struct tui_event){ .kind = TUI_KEY_UP };
            case 'B':
                return (struct tui_event){ .kind = TUI_KEY_DOWN };
            case 'C':
                return (struct tui_event){ .kind = TUI_KEY_RIGHT };
            case 'D':
                return (struct tui_event){ .kind = TUI_KEY_LEFT };
            default:
                break;
            }
        }
        return (struct tui_event){ .kind = TUI_KEY_ESCAPE };
    }

    if (character == '\r' || character == '\n') {
        return (struct tui_event){ .kind = TUI_KEY_ENTER };
    }
    if (character == '\b' || character == 127) {
        return (struct tui_event){ .kind = TUI_KEY_BACKSPACE };
    }
    return (struct tui_event){
        .kind = TUI_KEY_CHARACTER,
        .character = character,
    };
}

bool tui_read_line(const char *prompt, char *buffer, size_t capacity)
{
    size_t length = 0;

    if (capacity == 0) {
        return false;
    }
    tui_show_cursor();
    buffer[0] = '\0';
    if (prompt && *prompt) {
        tui_printf("%s", prompt);
    }
    fflush(stdout);

    for (;;) {
        struct tui_event event = tui_read_key();
        if (event.kind == TUI_KEY_ENTER) {
            fputc('\n', stdout);
            tui_hide_cursor();
            return true;
        }
        if (event.kind == TUI_KEY_ESCAPE) {
            fputc('\n', stdout);
            tui_hide_cursor();
            return false;
        }
        if (event.kind == TUI_KEY_BACKSPACE) {
            if (length > 0) {
                buffer[--length] = '\0';
                fputs("\b \b", stdout);
                fflush(stdout);
            }
            continue;
        }
        if (event.kind == TUI_KEY_CHARACTER &&
            event.character >= 32 && event.character != 127 &&
            length + 1 < capacity) {
            buffer[length++] = (char)event.character;
            buffer[length] = '\0';
            fputc(event.character, stdout);
            fflush(stdout);
        }
    }
}

void tui_wait(void)
{
    fputs("\n\n按任意键返回...", stdout);
    fflush(stdout);
    (void)tui_read_key();
}
