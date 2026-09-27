/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#include "ui.h"

#include "polynomial.h"
#include "tui.h"

#include <ctype.h>
#include <errno.h>
#include <inttypes.h>
#include <limits.h>
#include <math.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define STYLE_RESET     "\033[0m"
#define STYLE_BOLD      "\033[1m"
#define STYLE_DIM       "\033[2m"
#define STYLE_SELECTED  "\033[7;1m"
#define COLOR_CYAN      "\033[1;36m"
#define COLOR_GREEN     "\033[1;32m"
#define COLOR_RED       "\033[1;31m"
#define COLOR_YELLOW    "\033[1;33m"

#define UI_LINE_WIDTH   50

enum ui_vector_result_kind {
    UI_VECTOR_RESULT_NONE,
    UI_VECTOR_RESULT_VALUE,
    UI_VECTOR_RESULT_COSINE,
};

struct ui_state {
    struct vec linear;

    struct vec vector_a;
    struct vec vector_b;
    struct vec vector_result;
    enum ui_vector_result_kind vector_result_kind;
    double vector_cosine;

    struct poly_vec poly_vec_a;
    struct poly_vec poly_vec_b;
    struct poly_vec poly_vec_result;
    bool poly_vec_result_valid;

    struct poly_list poly_list_a;
    struct poly_list poly_list_b;
    struct poly_list poly_list_result;
    bool poly_list_result_valid;

    char status[256];
};

static void ui_set_status(struct ui_state *state, const char *format, ...)
{
    va_list arguments;

    va_start(arguments, format);
    vsnprintf(state->status, sizeof(state->status), format, arguments);
    va_end(arguments);
}

static void ui_clear_status(struct ui_state *state)
{
    state->status[0] = '\0';
}

/* 计算 UTF-8 字符串在终端中的实际显示列宽（中文字符占 2 列） */
static size_t ui_str_display_width(const char *str)
{
    size_t width = 0;
    while (*str) {
        unsigned char c = (unsigned char)*str;
        if (c < 0x80) {
            width += 1;
            str += 1;
        } else if ((c & 0xE0) == 0xC0) {
            width += 1;
            str += 2;
        } else if ((c & 0xF0) == 0xE0) {
            width += 2;
            str += 3;
        } else if ((c & 0xF8) == 0xF0) {
            width += 2;
            str += 4;
        } else {
            str += 1;
        }
    }
    return width;
}

static void ui_print_centered(const char *text, int width)
{
    size_t disp_w = ui_str_display_width(text);
    int pad = (width > (int)disp_w) ? (width - (int)disp_w) / 2 : 0;
    for (int i = 0; i < pad; ++i) {
        putchar(' ');
    }
    printf("%s\n", text);
}

static void ui_print_header(const char *title)
{
    printf(COLOR_CYAN "==================================================" STYLE_RESET "\n");
    printf(STYLE_BOLD);
    ui_print_centered(title, UI_LINE_WIDTH);
    printf(STYLE_RESET);
    printf(COLOR_CYAN "==================================================" STYLE_RESET "\n\n");
}

static void ui_print_status(const struct ui_state *state)
{
    if (state->status[0] != '\0') {
        printf("\n" STYLE_DIM "──────────────────────────────────────────────────" STYLE_RESET "\n");
        if (strncmp(state->status, "错误", 6) == 0) {
            printf(COLOR_RED "  [!] %s" STYLE_RESET "\n", state->status);
        } else if (strncmp(state->status, "已取消", 9) == 0) {
            printf(COLOR_YELLOW "  [*] %s" STYLE_RESET "\n", state->status);
        } else {
            printf(COLOR_GREEN "  [√] %s" STYLE_RESET "\n", state->status);
        }
    }
}

static void ui_print_footer(const char *text)
{
    printf("\n" STYLE_DIM "──────────────────────────────────────────────────" STYLE_RESET "\n");
    printf(STYLE_DIM "  %s" STYLE_RESET "\n", text);
}

static void ui_print_menu(const char *const *items,
                          size_t count,
                          size_t selected,
                          const char *zero_item)
{
    for (size_t i = 0; i < count; ++i) {
        if (i == selected) {
            printf(STYLE_SELECTED "  > %zu. %-32s " STYLE_RESET "\n", i + 1, items[i]);
        } else {
            printf("    %zu. %-32s\n", i + 1, items[i]);
        }
    }
    if (selected == count) {
        printf(STYLE_SELECTED "  > 0. %-32s " STYLE_RESET "\n", zero_item);
    } else {
        printf(STYLE_DIM "    0. %-32s" STYLE_RESET "\n", zero_item);
    }
}

static int ui_menu_input(size_t *selected, size_t count)
{
    size_t total = count + 1;
    struct tui_event event = tui_read_key();

    if (event.kind == TUI_KEY_UP) {
        *selected = *selected == 0 ? total - 1 : *selected - 1;
        return -2;
    }
    if (event.kind == TUI_KEY_DOWN) {
        *selected = (*selected + 1) % total;
        return -2;
    }
    if (event.kind == TUI_KEY_CHARACTER &&
        (event.character == 'k' || event.character == 'K')) {
        *selected = *selected == 0 ? total - 1 : *selected - 1;
        return -2;
    }
    if (event.kind == TUI_KEY_CHARACTER &&
        (event.character == 'j' || event.character == 'J')) {
        *selected = (*selected + 1) % total;
        return -2;
    }
    if (event.kind == TUI_KEY_ENTER) {
        if (*selected == count) {
            return -1;
        }
        return (int)*selected;
    }
    if (event.kind == TUI_KEY_ESCAPE ||
        (event.kind == TUI_KEY_CHARACTER &&
         (event.character == 'q' || event.character == 'Q' ||
          event.character == '0'))) {
        return -1;
    }
    if (event.kind == TUI_KEY_CHARACTER &&
        event.character >= '1' &&
        event.character <= '0' + (int)count) {
        return event.character - '1';
    }
    return -2;
}

static void ui_print_vec_int(const struct vec *vector)
{
    printf("[");
    for (size_t i = 0; i < vector->size; ++i) {
        if (i != 0) {
            printf(", ");
        }
        printf("%d", *(const int *)vec_at_const(vector, i));
    }
    printf("]");
}

static void ui_print_vec_double(const struct vec *vector)
{
    printf("[");
    for (size_t i = 0; i < vector->size; ++i) {
        if (i != 0) {
            printf(", ");
        }
        printf("%g", *(const double *)vec_at_const(vector, i));
    }
    printf("]");
}

static void ui_print_poly_term(double coefficient,
                               unsigned int exponent,
                               bool *first)
{
    double magnitude = fabs(coefficient);

    if (*first) {
        if (coefficient < 0.0) {
            printf("-");
        }
        *first = false;
    } else {
        printf(coefficient < 0.0 ? " - " : " + ");
    }

    if (exponent == 0) {
        printf("%g", magnitude);
        return;
    }
    if (magnitude != 1.0) {
        printf("%g", magnitude);
    }
    printf("x");
    if (exponent != 1) {
        printf("^%u", exponent);
    }
}

static void ui_print_poly_vec(const struct poly_vec *poly)
{
    bool first = true;

    for (size_t i = 0; i < poly->terms.size; ++i) {
        const struct poly_term *term = vec_at_const(&poly->terms, i);
        ui_print_poly_term(term->coef, term->exp, &first);
    }
    if (first) {
        printf("0");
    }
}

static void ui_print_poly_list(const struct poly_list *poly)
{
    bool first = true;
    const struct list_head *position = poly->head.next;

    while (position != &poly->head) {
        const struct poly_node *node =
            list_entry(position, const struct poly_node, link);
        ui_print_poly_term(node->term.coef, node->term.exp, &first);
        position = position->next;
    }
    if (first) {
        printf("0");
    }
}

static const char *ui_skip_space(const char *text)
{
    while (isspace((unsigned char)*text)) {
        ++text;
    }
    return text;
}

static bool ui_parse_int(const char *text, int *value)
{
    char *end;
    const char *start = ui_skip_space(text);

    errno = 0;
    long parsed = strtol(start, &end, 10);
    if (start == end || errno == ERANGE || parsed < INT_MIN ||
        parsed > INT_MAX || *ui_skip_space(end) != '\0') {
        return false;
    }
    *value = (int)parsed;
    return true;
}

static bool ui_parse_size(const char *text, size_t *value)
{
    char *end;
    const char *start = ui_skip_space(text);

    if (*start == '-') {
        return false;
    }
    errno = 0;
    uintmax_t parsed = strtoumax(start, &end, 10);
    if (start == end || errno == ERANGE || parsed > SIZE_MAX ||
        *ui_skip_space(end) != '\0') {
        return false;
    }
    *value = (size_t)parsed;
    return true;
}

static bool ui_parse_unsigned(const char *text, unsigned int *value)
{
    char *end;
    const char *start = ui_skip_space(text);

    if (*start == '-') {
        return false;
    }
    errno = 0;
    uintmax_t parsed = strtoumax(start, &end, 10);
    if (start == end || errno == ERANGE || parsed > UINT_MAX ||
        *ui_skip_space(end) != '\0') {
        return false;
    }
    *value = (unsigned int)parsed;
    return true;
}

static bool ui_parse_vector(const char *text, struct vec *destination)
{
    struct vec temporary;
    vec_init(&temporary, double);
    const char *position = text;

    for (;;) {
        char *end;
        position = ui_skip_space(position);
        if (*position == '\0') {
            break;
        }

        errno = 0;
        double value = strtod(position, &end);
        if (position == end || errno == ERANGE ||
            !vec_push_back(&temporary, &value)) {
            vec_destroy(&temporary);
            return false;
        }
        position = end;
    }

    if (temporary.size == 0) {
        vec_destroy(&temporary);
        return false;
    }
    vec_swap(destination, &temporary);
    vec_destroy(&temporary);
    return true;
}

static bool ui_parse_poly_vec(const char *text, struct poly_vec *destination)
{
    struct poly_vec temporary;
    poly_vec_init(&temporary);
    const char *position = text;
    size_t term_count = 0;

    for (;;) {
        char *end;
        position = ui_skip_space(position);
        if (*position == '\0') {
            break;
        }

        errno = 0;
        double coefficient = strtod(position, &end);
        if (position == end || errno == ERANGE) {
            poly_vec_destroy(&temporary);
            return false;
        }
        position = ui_skip_space(end);

        if (*position == '-') {
            poly_vec_destroy(&temporary);
            return false;
        }
        errno = 0;
        uintmax_t exponent = strtoumax(position, &end, 10);
        if (position == end || errno == ERANGE || exponent > UINT_MAX) {
            poly_vec_destroy(&temporary);
            return false;
        }
        if (!poly_vec_add_term(&temporary, coefficient,
                               (unsigned int)exponent)) {
            poly_vec_destroy(&temporary);
            return false;
        }
        ++term_count;
        position = end;
    }

    if (term_count == 0) {
        poly_vec_destroy(&temporary);
        return false;
    }
    vec_swap(&destination->terms, &temporary.terms);
    poly_vec_destroy(&temporary);
    return true;
}

static bool ui_parse_poly_list(const char *text, struct poly_list *destination)
{
    struct poly_list temporary;
    poly_list_init(&temporary);
    const char *position = text;
    size_t term_count = 0;

    for (;;) {
        char *end;
        position = ui_skip_space(position);
        if (*position == '\0') {
            break;
        }

        errno = 0;
        double coefficient = strtod(position, &end);
        if (position == end || errno == ERANGE) {
            poly_list_destroy(&temporary);
            return false;
        }
        position = ui_skip_space(end);

        if (*position == '-') {
            poly_list_destroy(&temporary);
            return false;
        }
        errno = 0;
        uintmax_t exponent = strtoumax(position, &end, 10);
        if (position == end || errno == ERANGE || exponent > UINT_MAX) {
            poly_list_destroy(&temporary);
            return false;
        }
        if (!poly_list_add_term(&temporary, coefficient,
                                (unsigned int)exponent)) {
            poly_list_destroy(&temporary);
            return false;
        }
        ++term_count;
        position = end;
    }

    if (term_count == 0) {
        poly_list_destroy(&temporary);
        return false;
    }
    __poly_list_replace(destination, &temporary);
    return true;
}

/* 原位输入框：保留界面上下文数据 */
static bool ui_prompt_inline(const char *title,
                             const char *description,
                             char *buffer,
                             size_t capacity)
{
    printf("\n" COLOR_YELLOW "┌── %s" STYLE_RESET "\n", title);
    if (description != NULL && *description != '\0') {
        printf(COLOR_YELLOW "│ " STYLE_RESET "%s\n", description);
    }
    printf(COLOR_YELLOW "└─> " STYLE_RESET);
    fflush(stdout);
    return tui_read_line("", buffer, capacity);
}

static bool ui_confirm(const char *title, const char *question)
{
    char input[16];

    if (!ui_prompt_inline(title, question, input, sizeof(input))) {
        return false;
    }
    return input[0] == 'y' || input[0] == 'Y';
}

static bool ui_state_init(struct ui_state *state)
{
    memset(state, 0, sizeof(*state));
    vec_init(&state->linear, int);
    vec_init(&state->vector_a, double);
    vec_init(&state->vector_b, double);
    vec_init(&state->vector_result, double);
    poly_vec_init(&state->poly_vec_a);
    poly_vec_init(&state->poly_vec_b);
    poly_vec_init(&state->poly_vec_result);
    poly_list_init(&state->poly_list_a);
    poly_list_init(&state->poly_list_b);
    poly_list_init(&state->poly_list_result);

    int linear_values[] = { 10, 20, 30, 40 };
    double vector_a_values[] = { 1.0, 2.0, 3.0 };
    double vector_b_values[] = { 4.0, 5.0, 6.0 };
    for (size_t i = 0; i < sizeof(linear_values) / sizeof(linear_values[0]);
         ++i) {
        if (!vec_push_back(&state->linear, &linear_values[i])) {
            return false;
        }
    }
    for (size_t i = 0; i < sizeof(vector_a_values) / sizeof(vector_a_values[0]);
         ++i) {
        if (!vec_push_back(&state->vector_a, &vector_a_values[i]) ||
            !vec_push_back(&state->vector_b, &vector_b_values[i])) {
            return false;
        }
    }

    if (!poly_vec_add_term(&state->poly_vec_a, 5.0, 4) ||
        !poly_vec_add_term(&state->poly_vec_a, 3.0, 2) ||
        !poly_vec_add_term(&state->poly_vec_a, 1.0, 0) ||
        !poly_vec_add_term(&state->poly_vec_b, 2.0, 3) ||
        !poly_vec_add_term(&state->poly_vec_b, -3.0, 2) ||
        !poly_vec_add_term(&state->poly_vec_b, 7.0, 0) ||
        !poly_list_add_term(&state->poly_list_a, 5.0, 4) ||
        !poly_list_add_term(&state->poly_list_a, 3.0, 2) ||
        !poly_list_add_term(&state->poly_list_a, 1.0, 0) ||
        !poly_list_add_term(&state->poly_list_b, 2.0, 3) ||
        !poly_list_add_term(&state->poly_list_b, -3.0, 2) ||
        !poly_list_add_term(&state->poly_list_b, 7.0, 0)) {
        return false;
    }
    return true;
}

static void ui_state_destroy(struct ui_state *state)
{
    vec_destroy(&state->linear);
    vec_destroy(&state->vector_a);
    vec_destroy(&state->vector_b);
    vec_destroy(&state->vector_result);
    poly_vec_destroy(&state->poly_vec_a);
    poly_vec_destroy(&state->poly_vec_b);
    poly_vec_destroy(&state->poly_vec_result);
    poly_list_destroy(&state->poly_list_a);
    poly_list_destroy(&state->poly_list_b);
    poly_list_destroy(&state->poly_list_result);
}

static void ui_draw_linear(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "尾部添加",
        "指定位置插入",
        "删除元素",
        "修改元素",
        "查找元素",
        "遍历线性表",
        "清空线性表",
        "查看内部状态",
    };

    tui_clear();
    ui_print_header("线性表操作");
    printf(COLOR_CYAN "当前线性表: " STYLE_BOLD);
    ui_print_vec_int(&state->linear);
    printf(STYLE_RESET "\n" STYLE_DIM "size = %zu    capacity = %zu" STYLE_RESET "\n\n",
           state->linear.size, state->linear.capacity);

    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "返回");
    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static bool ui_read_int_value(const char *title,
                              const char *description,
                              int *value,
                              struct ui_state *state)
{
    char input[128];

    if (!ui_prompt_inline(title, description, input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return false;
    }
    if (!ui_parse_int(input, value)) {
        ui_set_status(state, "错误：输入格式无效");
        return false;
    }
    return true;
}

static bool ui_read_size_value(const char *title,
                               const char *description,
                               size_t *value,
                               struct ui_state *state)
{
    char input[128];

    if (!ui_prompt_inline(title, description, input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return false;
    }
    if (!ui_parse_size(input, value)) {
        ui_set_status(state, "错误：输入格式无效");
        return false;
    }
    return true;
}

static void ui_linear_append(struct ui_state *state)
{
    int value;

    if (!ui_read_int_value("尾部添加", "元素值：", &value, state)) {
        return;
    }
    if (!vec_push_back(&state->linear, &value)) {
        ui_set_status(state, "错误：内存分配失败");
        return;
    }
    ui_set_status(state, "已添加元素 %d", value);
}

static void ui_linear_insert(struct ui_state *state)
{
    size_t index;
    int value;

    if (!ui_read_size_value("指定位置插入", "插入位置：", &index, state) ||
        !ui_read_int_value("指定位置插入", "元素值：", &value, state)) {
        return;
    }
    if (index > state->linear.size) {
        ui_set_status(state, "错误：插入位置超出范围");
        return;
    }
    if (!vec_insert(&state->linear, index, &value)) {
        ui_set_status(state, "错误：内存分配失败");
        return;
    }
    ui_set_status(state, "已在位置 %zu 插入 %d", index, value);
}

static void ui_linear_erase(struct ui_state *state)
{
    size_t index;

    if (!ui_read_size_value("删除元素", "删除位置：", &index, state)) {
        return;
    }
    if (index >= state->linear.size) {
        ui_set_status(state, "错误：删除位置超出范围");
        return;
    }
    vec_erase(&state->linear, index);
    ui_set_status(state, "已删除位置 %zu 的元素", index);
}

static void ui_linear_modify(struct ui_state *state)
{
    size_t index;
    int value;

    if (!ui_read_size_value("修改元素", "修改位置：", &index, state) ||
        !ui_read_int_value("修改元素", "新元素值：", &value, state)) {
        return;
    }
    int *element = vec_at_checked(&state->linear, index);
    if (element == NULL) {
        ui_set_status(state, "错误：修改位置超出范围");
        return;
    }
    *element = value;
    ui_set_status(state, "已修改位置 %zu", index);
}

static void ui_linear_find(struct ui_state *state)
{
    int value;

    if (!ui_read_int_value("查找元素", "查找值：", &value, state)) {
        return;
    }
    for (size_t i = 0; i < state->linear.size; ++i) {
        if (*(const int *)vec_at_const(&state->linear, i) == value) {
            ui_set_status(state, "元素 %d 位于位置 %zu", value, i);
            return;
        }
    }
    ui_set_status(state, "未找到元素 %d", value);
}

static void ui_linear_traverse(const struct ui_state *state)
{
    tui_clear();
    ui_print_header("遍历线性表");
    printf("当前线性表：");
    ui_print_vec_int(&state->linear);
    printf("\n");
    tui_wait();
}

static void ui_linear_clear(struct ui_state *state)
{
    if (!ui_confirm("清空线性表", "确定清空当前线性表？ [y/N]")) {
        ui_set_status(state, "已取消");
        return;
    }
    vec_clear(&state->linear);
    ui_set_status(state, "已清空线性表");
}

static void ui_linear_state(const struct ui_state *state)
{
    tui_clear();
    ui_print_header("线性表内部状态");
    printf("size      = %zu\n", state->linear.size);
    printf("capacity  = %zu\n", state->linear.capacity);
    printf("elem_size = %zu\n\n", state->linear.elem_size);
    printf("下标        元素\n");
    printf("────────────────────────\n");
    for (size_t i = 0; i < state->linear.size; ++i) {
        printf("%-11zu %d\n", i,
               *(const int *)vec_at_const(&state->linear, i));
    }
    tui_wait();
}

static void ui_linear_action(struct ui_state *state, int choice)
{
    ui_clear_status(state);
    switch (choice) {
    case 0:
        ui_linear_append(state);
        break;
    case 1:
        ui_linear_insert(state);
        break;
    case 2:
        ui_linear_erase(state);
        break;
    case 3:
        ui_linear_modify(state);
        break;
    case 4:
        ui_linear_find(state);
        break;
    case 5:
        ui_linear_traverse(state);
        break;
    case 6:
        ui_linear_clear(state);
        break;
    case 7:
        ui_linear_state(state);
        break;
    default:
        break;
    }
}

static void ui_linear_page(struct ui_state *state)
{
    size_t selected = 0;

    for (;;) {
        ui_draw_linear(state, selected);
        int choice = ui_menu_input(&selected, 8);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_linear_action(state, choice);
        }
    }
}

static void ui_draw_vector(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "输入向量 A",
        "输入向量 B",
        "A + B",
        "A - B",
        "计算夹角余弦",
    };

    tui_clear();
    ui_print_header("向量运算");
    printf(COLOR_CYAN "A = " STYLE_BOLD);
    ui_print_vec_double(&state->vector_a);
    printf(STYLE_RESET "\n" COLOR_CYAN "B = " STYLE_BOLD);
    ui_print_vec_double(&state->vector_b);
    printf(STYLE_RESET "\n\n");

    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "返回");

    if (state->vector_result_kind == UI_VECTOR_RESULT_VALUE) {
        printf("\n" COLOR_GREEN "结果：" STYLE_BOLD);
        ui_print_vec_double(&state->vector_result);
        printf(STYLE_RESET "\n");
    } else if (state->vector_result_kind == UI_VECTOR_RESULT_COSINE) {
        printf("\n" COLOR_GREEN "cos(A, B) = %.6f" STYLE_RESET "\n", state->vector_cosine);
    }

    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static void ui_vector_input(struct vec *destination,
                            const char *name,
                            struct ui_state *state)
{
    char input[512];
    char description[128];

    snprintf(description, sizeof(description),
             "请输入各分量，以空格分隔（向量 %s）：", name);
    if (!ui_prompt_inline(name, description, input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_vector(input, destination)) {
        ui_set_status(state, "错误：向量输入格式无效");
        return;
    }
    state->vector_result_kind = UI_VECTOR_RESULT_NONE;
    ui_set_status(state, "已更新向量 %s", name);
}

static void ui_vector_binary(struct ui_state *state, bool subtract)
{
    if (state->vector_a.size != state->vector_b.size) {
        ui_set_status(state, "错误：两个向量维度不同");
        return;
    }

    struct vec temporary;
    vec_init(&temporary, double);
    for (size_t i = 0; i < state->vector_a.size; ++i) {
        double left = *(const double *)vec_at_const(&state->vector_a, i);
        double right = *(const double *)vec_at_const(&state->vector_b, i);
        double value = subtract ? left - right : left + right;
        if (!vec_push_back(&temporary, &value)) {
            vec_destroy(&temporary);
            ui_set_status(state, "错误：内存分配失败");
            return;
        }
    }
    vec_swap(&state->vector_result, &temporary);
    vec_destroy(&temporary);
    state->vector_result_kind = UI_VECTOR_RESULT_VALUE;
    ui_set_status(state, "已计算 A %c B", subtract ? '-' : '+');
}

static void ui_vector_cosine(struct ui_state *state)
{
    if (state->vector_a.size != state->vector_b.size) {
        ui_set_status(state, "错误：两个向量维度不同");
        return;
    }

    double dot = 0.0;
    double norm_a = 0.0;
    double norm_b = 0.0;
    for (size_t i = 0; i < state->vector_a.size; ++i) {
        double left = *(const double *)vec_at_const(&state->vector_a, i);
        double right = *(const double *)vec_at_const(&state->vector_b, i);
        dot += left * right;
        norm_a += left * left;
        norm_b += right * right;
    }
    if (norm_a == 0.0 || norm_b == 0.0) {
        ui_set_status(state, "错误：零向量无法计算夹角余弦");
        return;
    }
    state->vector_cosine = dot / (sqrt(norm_a) * sqrt(norm_b));
    state->vector_result_kind = UI_VECTOR_RESULT_COSINE;
    ui_set_status(state, "已计算夹角余弦");
}

static void ui_vector_action(struct ui_state *state, int choice)
{
    ui_clear_status(state);
    switch (choice) {
    case 0:
        ui_vector_input(&state->vector_a, "输入向量 A", state);
        break;
    case 1:
        ui_vector_input(&state->vector_b, "输入向量 B", state);
        break;
    case 2:
        ui_vector_binary(state, false);
        break;
    case 3:
        ui_vector_binary(state, true);
        break;
    case 4:
        ui_vector_cosine(state);
        break;
    default:
        break;
    }
}

static void ui_vector_page(struct ui_state *state)
{
    size_t selected = 0;

    for (;;) {
        ui_draw_vector(state, selected);
        int choice = ui_menu_input(&selected, 5);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_vector_action(state, choice);
        }
    }
}

static void ui_draw_poly_vec(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "输入多项式 A",
        "输入多项式 B",
        "A + B",
        "A - B",
        "A × B",
        "多项式求导",
        "查看存储结构",
    };

    tui_clear();
    ui_print_header("顺序表多项式");
    printf(COLOR_CYAN "A(x) = " STYLE_BOLD);
    ui_print_poly_vec(&state->poly_vec_a);
    printf(STYLE_RESET "\n" COLOR_CYAN "B(x) = " STYLE_BOLD);
    ui_print_poly_vec(&state->poly_vec_b);
    printf(STYLE_RESET "\n\n");

    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "返回");

    if (state->poly_vec_result_valid) {
        printf("\n" COLOR_GREEN "结果：" STYLE_BOLD);
        ui_print_poly_vec(&state->poly_vec_result);
        printf(STYLE_RESET "\n");
    }

    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static void ui_poly_vec_input(struct poly_vec *destination,
                              const char *name,
                              struct ui_state *state)
{
    char input[512];
    char description[256];

    snprintf(description, sizeof(description),
             "输入格式：系数 指数；多个项用空格分隔\n例如：5 4  3 2  1 0\n输入多项式 %s：",
             name);
    if (!ui_prompt_inline("输入多项式", description, input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_poly_vec(input, destination)) {
        ui_set_status(state, "错误：输入格式无效，应输入“系数 指数”对");
        return;
    }
    state->poly_vec_result_valid = false;
    ui_set_status(state, "已更新多项式 %s", name);
}

static void ui_poly_vec_binary(struct ui_state *state, char operation)
{
    bool success;

    switch (operation) {
    case '+':
        success = poly_vec_add(&state->poly_vec_result,
                               &state->poly_vec_a, &state->poly_vec_b);
        break;
    case '-':
        success = poly_vec_sub(&state->poly_vec_result,
                               &state->poly_vec_a, &state->poly_vec_b);
        break;
    default:
        success = poly_vec_mul(&state->poly_vec_result,
                               &state->poly_vec_a, &state->poly_vec_b);
        break;
    }
    if (!success) {
        ui_set_status(state, "错误：多项式运算失败");
        return;
    }
    state->poly_vec_result_valid = true;
    ui_set_status(state, "已计算 A %c B", operation);
}

static void ui_poly_vec_derivative(struct ui_state *state)
{
    char input[128];
    unsigned int which;
    unsigned int order;
    const struct poly_vec *source;

    if (!ui_prompt_inline("多项式求导", "选择多项式（1=A，2=B）：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &which) || (which != 1 && which != 2)) {
        ui_set_status(state, "错误：多项式选择无效");
        return;
    }
    if (!ui_prompt_inline("多项式求导", "输入求导阶数：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &order)) {
        ui_set_status(state, "错误：求导阶数无效");
        return;
    }
    source = which == 1 ? &state->poly_vec_a : &state->poly_vec_b;
    if (!poly_vec_derivative(&state->poly_vec_result, source, order)) {
        ui_set_status(state, "错误：多项式求导失败");
        return;
    }
    state->poly_vec_result_valid = true;
    ui_set_status(state, "已完成多项式求导");
}

static void ui_poly_vec_storage(const struct ui_state *state)
{
    tui_clear();
    ui_print_header("顺序表存储结构");
    printf("A(x) = ");
    ui_print_poly_vec(&state->poly_vec_a);
    printf("\nsize = %zu    capacity = %zu\n",
           state->poly_vec_a.terms.size,
           state->poly_vec_a.terms.capacity);
    printf("下标         系数         指数\n");
    printf("─────────────────────────────\n");
    for (size_t i = 0; i < state->poly_vec_a.terms.size; ++i) {
        const struct poly_term *term =
            vec_at_const(&state->poly_vec_a.terms, i);
        printf("%-12zu %-12.6g %u\n", i, term->coef, term->exp);
    }
    printf("\nB(x) = ");
    ui_print_poly_vec(&state->poly_vec_b);
    printf("\nsize = %zu    capacity = %zu\n",
           state->poly_vec_b.terms.size,
           state->poly_vec_b.terms.capacity);
    tui_wait();
}

static void ui_poly_vec_action(struct ui_state *state, int choice)
{
    ui_clear_status(state);
    switch (choice) {
    case 0:
        ui_poly_vec_input(&state->poly_vec_a, "A", state);
        break;
    case 1:
        ui_poly_vec_input(&state->poly_vec_b, "B", state);
        break;
    case 2:
        ui_poly_vec_binary(state, '+');
        break;
    case 3:
        ui_poly_vec_binary(state, '-');
        break;
    case 4:
        ui_poly_vec_binary(state, '*');
        break;
    case 5:
        ui_poly_vec_derivative(state);
        break;
    case 6:
        ui_poly_vec_storage(state);
        break;
    default:
        break;
    }
}

static void ui_poly_vec_page(struct ui_state *state)
{
    size_t selected = 0;

    for (;;) {
        ui_draw_poly_vec(state, selected);
        int choice = ui_menu_input(&selected, 7);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_poly_vec_action(state, choice);
        }
    }
}

static void ui_draw_poly_list(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "输入多项式 A",
        "输入多项式 B",
        "A + B",
        "A - B",
        "A × B",
        "多项式求导",
        "查看链表结构",
    };

    tui_clear();
    ui_print_header("链表多项式");
    printf(COLOR_CYAN "A(x) = " STYLE_BOLD);
    ui_print_poly_list(&state->poly_list_a);
    printf(STYLE_RESET "\n" COLOR_CYAN "B(x) = " STYLE_BOLD);
    ui_print_poly_list(&state->poly_list_b);
    printf(STYLE_RESET "\n\n");

    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "返回");

    if (state->poly_list_result_valid) {
        printf("\n" COLOR_GREEN "结果：" STYLE_BOLD);
        ui_print_poly_list(&state->poly_list_result);
        printf(STYLE_RESET "\n");
    }

    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static void ui_poly_list_input(struct poly_list *destination,
                               const char *name,
                               struct ui_state *state)
{
    char input[512];
    char description[256];

    snprintf(description, sizeof(description),
             "输入格式：系数 指数；多个项用空格分隔\n例如：5 4  3 2  1 0\n输入多项式 %s：",
             name);
    if (!ui_prompt_inline("输入多项式", description, input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_poly_list(input, destination)) {
        ui_set_status(state, "错误：输入格式无效，应输入“系数 指数”对");
        return;
    }
    state->poly_list_result_valid = false;
    ui_set_status(state, "已更新多项式 %s", name);
}

static void ui_poly_list_binary(struct ui_state *state, char operation)
{
    bool success;

    switch (operation) {
    case '+':
        success = poly_list_add(&state->poly_list_result,
                                &state->poly_list_a, &state->poly_list_b);
        break;
    case '-':
        success = poly_list_sub(&state->poly_list_result,
                                &state->poly_list_a, &state->poly_list_b);
        break;
    default:
        success = poly_list_mul(&state->poly_list_result,
                                &state->poly_list_a, &state->poly_list_b);
        break;
    }
    if (!success) {
        ui_set_status(state, "错误：多项式运算失败");
        return;
    }
    state->poly_list_result_valid = true;
    ui_set_status(state, "已计算 A %c B", operation);
}

static void ui_poly_list_derivative(struct ui_state *state)
{
    char input[128];
    unsigned int which;
    unsigned int order;
    const struct poly_list *source;

    if (!ui_prompt_inline("多项式求导", "选择多项式（1=A，2=B）：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &which) || (which != 1 && which != 2)) {
        ui_set_status(state, "错误：多项式选择无效");
        return;
    }
    if (!ui_prompt_inline("多项式求导", "输入求导阶数：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &order)) {
        ui_set_status(state, "错误：求导阶数无效");
        return;
    }
    source = which == 1 ? &state->poly_list_a : &state->poly_list_b;
    if (!poly_list_derivative(&state->poly_list_result, source, order)) {
        ui_set_status(state, "错误：多项式求导失败");
        return;
    }
    state->poly_list_result_valid = true;
    ui_set_status(state, "已完成多项式求导");
}

static void ui_poly_list_storage(const struct ui_state *state)
{
    tui_clear();
    ui_print_header("链表存储结构");
    printf("A(x) = ");
    ui_print_poly_list(&state->poly_list_a);
    printf("\n节点数：%zu\n\n", state->poly_list_a.size);
    printf("HEAD");
    const struct list_head *position = state->poly_list_a.head.next;
    while (position != &state->poly_list_a.head) {
        const struct poly_node *node =
            list_entry(position, const struct poly_node, link);
        printf(" <-> [ %g, %u ]", node->term.coef, node->term.exp);
        position = position->next;
    }
    printf(" <-> HEAD\n");
    printf("\n每个节点：[ coef | exp | link ]\n");
    printf("\nB(x) = ");
    ui_print_poly_list(&state->poly_list_b);
    printf("\n节点数：%zu\n", state->poly_list_b.size);
    tui_wait();
}

static void ui_poly_list_action(struct ui_state *state, int choice)
{
    ui_clear_status(state);
    switch (choice) {
    case 0:
        ui_poly_list_input(&state->poly_list_a, "A", state);
        break;
    case 1:
        ui_poly_list_input(&state->poly_list_b, "B", state);
        break;
    case 2:
        ui_poly_list_binary(state, '+');
        break;
    case 3:
        ui_poly_list_binary(state, '-');
        break;
    case 4:
        ui_poly_list_binary(state, '*');
        break;
    case 5:
        ui_poly_list_derivative(state);
        break;
    case 6:
        ui_poly_list_storage(state);
        break;
    default:
        break;
    }
}

static void ui_poly_list_page(struct ui_state *state)
{
    size_t selected = 0;

    for (;;) {
        ui_draw_poly_list(state, selected);
        int choice = ui_menu_input(&selected, 7);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_poly_list_action(state, choice);
        }
    }
}

static void ui_draw_main(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "线性表操作",
        "向量运算",
        "顺序表多项式",
        "链表多项式",
    };

    (void)state;
    tui_clear();
    printf(COLOR_CYAN "==================================================" STYLE_RESET "\n");
    printf(STYLE_BOLD);
    ui_print_centered("简单计算器", UI_LINE_WIDTH);
    printf(STYLE_RESET);
    printf(COLOR_CYAN "==================================================" STYLE_RESET "\n\n");
    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "退出");
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q 退出");
    fflush(stdout);
}

int ui_run(void)
{
    struct ui_state state;

    if (!ui_state_init(&state)) {
        ui_state_destroy(&state);
        fputs("初始化数据失败\n", stderr);
        return 1;
    }
    if (!tui_init()) {
        ui_state_destroy(&state);
        fputs("无法初始化终端交互\n", stderr);
        return 1;
    }

    size_t selected = 0;
    for (;;) {
        ui_draw_main(&state, selected);
        int choice = ui_menu_input(&selected, 4);
        if (choice == -1) {
            break;
        }
        switch (choice) {
        case 0:
            ui_linear_page(&state);
            break;
        case 1:
            ui_vector_page(&state);
            break;
        case 2:
            ui_poly_vec_page(&state);
            break;
        case 3:
            ui_poly_list_page(&state);
            break;
        default:
            break;
        }
    }

    tui_shutdown();
    ui_state_destroy(&state);
    return 0;
}
