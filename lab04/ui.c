/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#include "ui.h"

#include "polynomial.h"
#include "expression.h"
#include "matrix.h"
#include "vector_math.h"
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
#include <sys/ioctl.h>
#include <unistd.h>

#define STYLE_RESET     "\033[0m"
#define STYLE_BOLD      "\033[1m"
#define STYLE_DIM       "\033[2m"
#define STYLE_SELECTED  "\033[7;1m"
#define COLOR_CYAN      "\033[1;36m"
#define COLOR_GREEN     "\033[1;32m"
#define COLOR_RED       "\033[1;31m"
#define COLOR_YELLOW    "\033[1;33m"

enum ui_vector_result_kind {
    UI_VECTOR_RESULT_NONE,
    UI_VECTOR_RESULT_VALUE,
    UI_VECTOR_RESULT_COSINE,
};

enum ui_matrix_result_kind {
    UI_MATRIX_RESULT_NONE,
    UI_MATRIX_RESULT_VALUE,
    UI_MATRIX_RESULT_DETERMINANT,
    UI_MATRIX_RESULT_EIGENVALUES,
};

struct ui_state {
    struct vec linear;

    struct vec vector_a;
    struct vec vector_b;
    struct vec vector_result;
    enum ui_vector_result_kind vector_result_kind;
    double vector_cosine;

    struct matrix matrix_a;
    struct matrix matrix_b;
    struct matrix matrix_result;
    struct vec matrix_eigenvalues;
    enum ui_matrix_result_kind matrix_result_kind;
    double matrix_determinant;

    struct poly_vec poly_vec_a;
    struct poly_vec poly_vec_b;
    struct poly_vec poly_vec_result;
    bool poly_vec_result_valid;

    struct poly_list poly_list_a;
    struct poly_list poly_list_b;
    struct poly_list poly_list_result;
    bool poly_list_result_valid;

    struct expression_context expression;
    char expression_text[EXPRESSION_TEXT_MAX];
    double expression_result;
    bool expression_result_valid;

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

static int ui_terminal_width(void)
{
    struct winsize size;
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0 && size.ws_col > 0) {
        return size.ws_col;
    }
    return 50;
}

static void ui_print_rule(char character)
{
    int width = ui_terminal_width();
    for (int i = 0; i < width; ++i) {
        putchar(character);
    }
    putchar('\n');
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
    int width = ui_terminal_width();
    printf(COLOR_CYAN);
    ui_print_rule('=');
    printf(STYLE_BOLD);
    ui_print_centered(title, width);
    printf(STYLE_RESET);
    printf(COLOR_CYAN);
    ui_print_rule('=');
    printf(STYLE_RESET "\n");
}

static void ui_print_status(const struct ui_state *state)
{
    if (state->status[0] != '\0') {
        printf("\n" STYLE_DIM);
        ui_print_rule('-');
        printf(STYLE_RESET);
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
    printf("\n" STYLE_DIM);
    ui_print_rule('-');
    printf(STYLE_RESET);
    printf(STYLE_DIM "  %s" STYLE_RESET "\n", text);
}

static void ui_print_menu(const char *const *items,
                          size_t count,
                          size_t selected,
                          const char *zero_item)
{
    for (size_t i = 0; i < count; ++i) {
        if (i == selected) {
            printf(STYLE_SELECTED "  > %zu. %s " STYLE_RESET "\n", i + 1, items[i]);
        } else {
            printf("    %zu. %s\n", i + 1, items[i]);
        }
    }
    if (selected == count) {
        printf(STYLE_SELECTED "  > 0. %s " STYLE_RESET "\n", zero_item);
    } else {
        printf(STYLE_DIM "    0. %s" STYLE_RESET "\n", zero_item);
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
    matrix_init(&state->matrix_a);
    matrix_init(&state->matrix_b);
    matrix_init(&state->matrix_result);
    vec_init(&state->matrix_eigenvalues, struct matrix_eigenvalue);
    poly_vec_init(&state->poly_vec_a);
    poly_vec_init(&state->poly_vec_b);
    poly_vec_init(&state->poly_vec_result);
    poly_list_init(&state->poly_list_a);
    poly_list_init(&state->poly_list_b);
    poly_list_init(&state->poly_list_result);
    expression_init(&state->expression);

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
    matrix_destroy(&state->matrix_a);
    matrix_destroy(&state->matrix_b);
    matrix_destroy(&state->matrix_result);
    vec_destroy(&state->matrix_eigenvalues);
    poly_vec_destroy(&state->poly_vec_a);
    poly_vec_destroy(&state->poly_vec_b);
    poly_vec_destroy(&state->poly_vec_result);
    poly_list_destroy(&state->poly_list_a);
    poly_list_destroy(&state->poly_list_b);
    poly_list_destroy(&state->poly_list_result);
    expression_destroy(&state->expression);
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
    bool success = subtract ?
        vector_sub(&state->vector_result, &state->vector_a, &state->vector_b) :
        vector_add(&state->vector_result, &state->vector_a, &state->vector_b);
    if (!success && state->vector_a.size != state->vector_b.size) {
        ui_set_status(state, "错误：两个向量维度不同");
        return;
    }
    if (!success) {
        ui_set_status(state, "错误：内存分配失败");
        return;
    }
    state->vector_result_kind = UI_VECTOR_RESULT_VALUE;
    ui_set_status(state, "已计算 A %c B", subtract ? '-' : '+');
}

static void ui_vector_cosine(struct ui_state *state)
{
    if (state->vector_a.size != state->vector_b.size) {
        ui_set_status(state, "错误：两个向量维度不同");
        return;
    }
    if (!vector_cosine(&state->vector_a, &state->vector_b,
                       &state->vector_cosine)) {
        ui_set_status(state, "错误：零向量无法计算夹角余弦");
        return;
    }
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

static void ui_print_matrix(const struct matrix *matrix)
{
    if (matrix->rows == 0) {
        printf("（未输入）\n");
        return;
    }
    printf("%zu × %zu\n", matrix->rows, matrix->cols);
    for (size_t row = 0; row < matrix->rows; ++row) {
        printf("  [");
        for (size_t col = 0; col < matrix->cols; ++col) {
            printf(" %12.6g", *matrix_at_const(matrix, row, col));
        }
        printf(" ]\n");
    }
}

static void ui_draw_matrix(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "输入矩阵 A",
        "输入矩阵 B",
        "A + B",
        "A - B",
        "A × B",
        "转置矩阵",
        "计算行列式",
        "计算特征值",
        "计算逆矩阵",
    };

    tui_clear();
    ui_print_header("矩阵运算");
    printf(COLOR_CYAN "A = " STYLE_RESET);
    ui_print_matrix(&state->matrix_a);
    printf(COLOR_CYAN "B = " STYLE_RESET);
    ui_print_matrix(&state->matrix_b);
    putchar('\n');
    ui_print_menu(items, sizeof(items) / sizeof(items[0]), selected, "返回");

    switch (state->matrix_result_kind) {
    case UI_MATRIX_RESULT_VALUE:
        printf("\n" COLOR_GREEN "结果：" STYLE_RESET);
        ui_print_matrix(&state->matrix_result);
        break;
    case UI_MATRIX_RESULT_DETERMINANT:
        printf("\n" COLOR_GREEN "行列式 = %.12g" STYLE_RESET "\n",
               state->matrix_determinant);
        break;
    case UI_MATRIX_RESULT_EIGENVALUES:
        printf("\n" COLOR_GREEN "特征值：" STYLE_RESET "\n");
        for (size_t i = 0; i < state->matrix_eigenvalues.size; ++i) {
            const struct matrix_eigenvalue *value =
                vec_at_const(&state->matrix_eigenvalues, i);
            printf("  λ%zu = %.12g", i + 1, value->real);
            if (value->imag != 0.0) printf("%+.12gi", value->imag);
            putchar('\n');
        }
        break;
    case UI_MATRIX_RESULT_NONE:
        break;
    }
    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static void ui_matrix_input(struct matrix *destination, const char *name,
                            struct ui_state *state)
{
    size_t rows, cols;
    if (!ui_read_size_value(name, "行数（正整数）：", &rows, state) ||
        !ui_read_size_value(name, "列数（正整数）：", &cols, state)) {
        return;
    }
    if (rows == 0 || cols == 0) {
        ui_set_status(state, "错误：矩阵行数和列数必须为正整数");
        return;
    }

    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, rows, cols)) {
        ui_set_status(state, "错误：矩阵尺寸过大或内存分配失败");
        return;
    }
    VEC(row, double);
    for (size_t i = 0; i < rows; ++i) {
        char input[512];
        char description[128];
        snprintf(description, sizeof(description),
                 "第 %zu/%zu 行：输入 %zu 个实数，以空格分隔", i + 1, rows, cols);
        if (!ui_prompt_inline(name, description, input, sizeof(input))) {
            ui_set_status(state, "已取消");
            goto done;
        }
        if (!ui_parse_vector(input, &row) || row.size != cols) {
            ui_set_status(state, "错误：每行必须输入 %zu 个实数", cols);
            goto done;
        }
        for (size_t j = 0; j < cols; ++j) {
            double value = *(const double *)vec_at_const(&row, j);
            if (!isfinite(value)) {
                ui_set_status(state, "错误：矩阵元素必须是有限实数");
                goto done;
            }
            *matrix_at(&temporary, i, j) = value;
        }
    }
    struct matrix old = *destination;
    *destination = temporary;
    temporary = old;
    ui_set_status(state, "已更新%s", name);
done:
    vec_destroy(&row);
    matrix_destroy(&temporary);
}

static void ui_matrix_binary(struct ui_state *state, char operation)
{
    const struct matrix *a = &state->matrix_a;
    const struct matrix *b = &state->matrix_b;
    if (a->rows == 0 || b->rows == 0) {
        ui_set_status(state, "错误：请先输入矩阵 A 和 B");
        return;
    }
    if (operation == '*' ? a->cols != b->rows :
        (a->rows != b->rows || a->cols != b->cols)) {
        ui_set_status(state, operation == '*' ?
                      "错误：A 的列数必须等于 B 的行数" :
                      "错误：两个矩阵的行数和列数必须相同");
        return;
    }
    bool success;
    switch (operation) {
    case '+': success = matrix_add(&state->matrix_result, a, b); break;
    case '-': success = matrix_sub(&state->matrix_result, a, b); break;
    default: success = matrix_mul(&state->matrix_result, a, b); break;
    }
    if (!success) {
        ui_set_status(state, "错误：结果尺寸过大或内存分配失败");
        return;
    }
    state->matrix_result_kind = UI_MATRIX_RESULT_VALUE;
    ui_set_status(state, "已计算 A %c B", operation);
}

static void ui_matrix_unary(struct ui_state *state, int choice)
{
    const char *title = choice == 5 ? "转置矩阵" :
                        choice == 6 ? "计算行列式" :
                        choice == 7 ? "计算特征值" : "计算逆矩阵";
    size_t selected;
    if (!ui_read_size_value(title, "选择矩阵：1 = A，2 = B", &selected, state)) {
        return;
    }
    if (selected != 1 && selected != 2) {
        ui_set_status(state, "错误：请输入 1 或 2");
        return;
    }
    const struct matrix *matrix = selected == 1 ? &state->matrix_a : &state->matrix_b;
    if (matrix->rows == 0) {
        ui_set_status(state, "错误：请先输入所选矩阵");
        return;
    }
    if (choice != 5 && matrix->rows != matrix->cols) {
        ui_set_status(state, "错误：行列式、特征值和求逆要求方阵");
        return;
    }
    bool success;
    enum ui_matrix_result_kind kind;
    switch (choice) {
    case 5:
        success = matrix_transpose(&state->matrix_result, matrix);
        kind = UI_MATRIX_RESULT_VALUE;
        break;
    case 6:
        success = matrix_det(matrix, &state->matrix_determinant);
        kind = UI_MATRIX_RESULT_DETERMINANT;
        break;
    case 7:
        success = matrix_eigenvalues(matrix, &state->matrix_eigenvalues);
        kind = UI_MATRIX_RESULT_EIGENVALUES;
        break;
    default:
        success = matrix_inverse(&state->matrix_result, matrix);
        kind = UI_MATRIX_RESULT_VALUE;
        break;
    }
    if (!success) {
        ui_set_status(state, choice == 7 ?
                      "错误：特征值计算失败（内存不足或迭代未收敛）" :
                      choice == 8 ? "错误：矩阵不可逆或内存分配失败" :
                      "错误：结果尺寸过大或内存分配失败");
        return;
    }
    state->matrix_result_kind = kind;
    ui_set_status(state, "%s完成（矩阵 %c）", title, selected == 1 ? 'A' : 'B');
}

static void ui_matrix_action(struct ui_state *state, int choice)
{
    ui_clear_status(state);
    state->matrix_result_kind = UI_MATRIX_RESULT_NONE;
    switch (choice) {
    case 0: ui_matrix_input(&state->matrix_a, "矩阵 A", state); break;
    case 1: ui_matrix_input(&state->matrix_b, "矩阵 B", state); break;
    case 2: ui_matrix_binary(state, '+'); break;
    case 3: ui_matrix_binary(state, '-'); break;
    case 4: ui_matrix_binary(state, '*'); break;
    case 5:
    case 6:
    case 7: ui_matrix_unary(state, choice); break;
    case 8: ui_matrix_unary(state, choice); break;
    }
}

static void ui_matrix_page(struct ui_state *state)
{
    size_t selected = 0;
    for (;;) {
        ui_draw_matrix(state, selected);
        int choice = ui_menu_input(&selected, 9);
        if (choice == -1) return;
        if (choice >= 0) ui_matrix_action(state, choice);
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
        "多项式积分",
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

static void ui_poly_vec_integral(struct ui_state *state)
{
    char input[128];
    unsigned int which;
    const struct poly_vec *source;

    if (!ui_prompt_inline("多项式积分", "选择多项式（1=A，2=B）：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &which) || (which != 1 && which != 2)) {
        ui_set_status(state, "错误：多项式选择无效");
        return;
    }
    source = which == 1 ? &state->poly_vec_a : &state->poly_vec_b;
    if (!poly_vec_integral(&state->poly_vec_result, source)) {
        ui_set_status(state, "错误：多项式积分失败，指数可能超出范围");
        return;
    }
    state->poly_vec_result_valid = true;
    ui_set_status(state, "已完成多项式积分（积分常数为 0）");
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
        ui_poly_vec_integral(state);
        break;
    case 7:
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
        int choice = ui_menu_input(&selected, 8);
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
        "多项式积分",
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

static void ui_poly_list_integral(struct ui_state *state)
{
    char input[128];
    unsigned int which;
    const struct poly_list *source;

    if (!ui_prompt_inline("多项式积分", "选择多项式（1=A，2=B）：",
                          input, sizeof(input))) {
        ui_set_status(state, "已取消");
        return;
    }
    if (!ui_parse_unsigned(input, &which) || (which != 1 && which != 2)) {
        ui_set_status(state, "错误：多项式选择无效");
        return;
    }
    source = which == 1 ? &state->poly_list_a : &state->poly_list_b;
    if (!poly_list_integral(&state->poly_list_result, source)) {
        ui_set_status(state, "错误：多项式积分失败，指数可能超出范围");
        return;
    }
    state->poly_list_result_valid = true;
    ui_set_status(state, "已完成多项式积分（积分常数为 0）");
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
        ui_poly_list_integral(state);
        break;
    case 7:
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
        int choice = ui_menu_input(&selected, 8);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_poly_list_action(state, choice);
        }
    }
}

static const char *ui_expression_error_text(enum expression_error_code code)
{
    switch (code) {
    case EXPRESSION_SYNTAX_ERROR: return "表达式语法错误";
    case EXPRESSION_PAREN_ERROR: return "括号不匹配";
    case EXPRESSION_UNKNOWN_VARIABLE: return "变量不存在";
    case EXPRESSION_UNSET_VARIABLE: return "变量尚未赋值";
    case EXPRESSION_DIVISION_BY_ZERO: return "除数不能为零";
    case EXPRESSION_UNKNOWN_FUNCTION: return "函数不存在";
    default: return "表达式错误";
    }
}

static void ui_draw_expression(const struct ui_state *state, size_t selected)
{
    static const char *const items[] = {
        "输入表达式或 DEF/RUN 命令",
        "修改变量值",
        "计算表达式",
        "查看并运行历史函数",
    };

    tui_clear();
    ui_print_header("表达式计算");
    printf(COLOR_CYAN "当前表达式：" STYLE_RESET "\n  %s\n\n",
           state->expression_text[0] ? state->expression_text : "（未输入）");
    printf(COLOR_CYAN "变量表：" STYLE_RESET "\n");
    if (state->expression.variables.size == 0) {
        printf("  （无变量）\n");
    }
    for (size_t i = 0; i < state->expression.variables.size; ++i) {
        const struct expression_variable *v =
            vec_at_const(&state->expression.variables, i);
        if (v->assigned) printf("  %s = %g\n", v->name, v->value);
        else printf("  %s = （未赋值）\n", v->name);
    }
    printf("\n" COLOR_CYAN "计算结果：" STYLE_RESET "\n");
    if (state->expression_result_valid) printf("  %g\n", state->expression_result);
    else printf("  （未计算）\n");
    ui_print_menu(items, 4, selected, "返回");
    ui_print_status(state);
    ui_print_footer("↑/↓ 或 j/k 选择    Enter 确认    q/Esc 返回");
    fflush(stdout);
}

static void ui_expression_calculate(struct ui_state *state);

static void ui_expression_input(struct ui_state *state)
{
    char input[sizeof(state->expression_text)];
    if (!ui_prompt_inline("输入表达式或命令",
                          "例如：X2^2 或 DEF f(x)=x<=1?1:x*f(x-1); 或 RUN f(5);",
                          input, sizeof(input))) return;
    struct expression_error error;
    const char *text = ui_skip_space(input);
    if (strncmp(text, "DEF", 3) == 0 &&
        !expression_name_char((unsigned char)text[3])) {
        if (!expression_define(&state->expression, text, &error)) {
            ui_set_status(state, "错误：%s（位置 %zu）",
                          ui_expression_error_text(error.code), error.position);
            return;
        }
        const struct expression_function *function =
            vec_back_const(&state->expression.functions);
        ui_set_status(state, "已定义函数 %s(%s)，历史记录 %zu",
                      function->name, function->parameter, state->expression.functions.size);
        return;
    }
    bool run = strncmp(text, "RUN", 3) == 0 &&
               !expression_name_char((unsigned char)text[3]);
    if (run) text = ui_skip_space(text + 3);
    size_t length = strlen(text);
    while (length > 0 && isspace((unsigned char)text[length - 1])) --length;
    if (length > 0 && text[length - 1] == ';') --length;
    memmove(input, text, length);
    input[length] = '\0';
    if (!expression_collect_variables(&state->expression, input, &error)) {
        ui_set_status(state, "错误：%s（位置 %zu）",
                      ui_expression_error_text(error.code), error.position);
        return;
    }
    strcpy(state->expression_text, input);
    state->expression_result_valid = false;
    ui_set_status(state, "已更新表达式");
    if (run) ui_expression_calculate(state);
}

static void ui_expression_set_variable(struct ui_state *state)
{
    char name[EXPRESSION_NAME_MAX];
    char input[64];
    if (state->expression.variables.size == 0) {
        ui_set_status(state, "当前表达式没有变量");
        return;
    }
    if (!ui_prompt_inline("修改变量值", "输入变量名：", name, sizeof(name))) return;
    struct expression_variable *variable = expression_variable(&state->expression, name);
    if (!variable) {
        ui_set_status(state, "错误：变量不存在");
        return;
    }
    if (!ui_prompt_inline("修改变量值", "输入新值：", input, sizeof(input))) return;
    char *end;
    errno = 0;
    double value = strtod(input, &end);
    if (end == input || errno == ERANGE || *ui_skip_space(end) != '\0') {
        ui_set_status(state, "错误：数值格式无效");
        return;
    }
    variable->value = value;
    variable->assigned = true;
    state->expression_result_valid = false;
    ui_set_status(state, "已更新变量 %s", name);
}

static void ui_expression_calculate(struct ui_state *state)
{
    if (state->expression_text[0] == '\0') {
        ui_set_status(state, "错误：请先输入表达式");
        return;
    }
    struct expression_error error;
    if (!expression_eval(&state->expression, state->expression_text,
                         &state->expression_result, &error)) {
        state->expression_result_valid = false;
        if (error.code == EXPRESSION_UNKNOWN_FUNCTION) {
            state->expression_text[0] = '\0';
            vec_clear(&state->expression.variables);
        }
        ui_set_status(state, "错误：%s（位置 %zu）",
                      ui_expression_error_text(error.code), error.position);
        return;
    }
    state->expression_result_valid = true;
    ui_set_status(state, "计算完成");
}

static void ui_expression_history(struct ui_state *state)
{
    if (state->expression.functions.size == 0) {
        ui_set_status(state, "尚未定义函数");
        return;
    }
    ui_clear_status(state);
    for (;;) {
        tui_clear();
        ui_print_header("函数定义历史");
        for (size_t i = 0; i < state->expression.functions.size; ++i) {
            const struct expression_function *function =
                vec_at_const(&state->expression.functions, i);
            printf("%zu. %s(%s) = %s\n", i + 1,
                   function->name, function->parameter, function->body);
        }
        ui_print_status(state);
        ui_print_footer("输入记录序号运行历史函数    Esc 返回");
        size_t choice;
        if (!ui_read_size_value("运行历史函数", "记录序号：", &choice, state)) return;
        if (choice == 0 || choice > state->expression.functions.size) {
            ui_set_status(state, "错误：记录序号超出范围");
            continue;
        }
        const struct expression_function *function =
            vec_at_const(&state->expression.functions, choice - 1);
        char input[64];
        char description[128];
        snprintf(description, sizeof(description), "输入形参 %s 的值：", function->parameter);
        if (!ui_prompt_inline("运行历史函数", description, input, sizeof(input))) return;
        char *end;
        errno = 0;
        double argument = strtod(input, &end);
        if (end == input || errno == ERANGE || *ui_skip_space(end) != '\0') {
            ui_set_status(state, "错误：数值格式无效");
            continue;
        }
        double result;
        struct expression_error error;
        if (!expression_call(&state->expression, choice - 1, argument, &result, &error)) {
            ui_set_status(state, "错误：%s（位置 %zu）",
                          ui_expression_error_text(error.code), error.position);
            continue;
        }
        printf("\n" COLOR_GREEN "历史记录 %zu：%s(%g) = %g" STYLE_RESET "\n",
               choice, function->name, argument, result);
        tui_wait();
        ui_clear_status(state);
    }
}

static void ui_expression_action(struct ui_state *state, int choice)
{
    switch (choice) {
    case 0:
        ui_expression_input(state);
        break;
    case 1:
        ui_expression_set_variable(state);
        break;
    case 2:
        ui_expression_calculate(state);
        break;
    case 3:
        ui_expression_history(state);
        break;
    default:
        break;
    }
}

static void ui_expression_page(struct ui_state *state)
{
    size_t selected = 0;
    for (;;) {
        ui_draw_expression(state, selected);
        int choice = ui_menu_input(&selected, 4);
        if (choice == -1) {
            return;
        }
        if (choice >= 0) {
            ui_expression_action(state, choice);
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
        "表达式计算",
        "矩阵运算",
    };

    (void)state;
    tui_clear();
    printf(COLOR_CYAN);
    ui_print_rule('=');
    printf(STYLE_BOLD);
    ui_print_centered("简单计算器", ui_terminal_width());
    printf(STYLE_RESET);
    printf(COLOR_CYAN);
    ui_print_rule('=');
    printf(STYLE_RESET "\n");
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
        int choice = ui_menu_input(&selected, 6);
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
        case 4:
            ui_expression_page(&state);
            break;
        case 5:
            ui_matrix_page(&state);
            break;
        default:
            break;
        }
    }

    tui_shutdown();
    ui_state_destroy(&state);
    return 0;
}
