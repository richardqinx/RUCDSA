/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <ctype.h>
#include <errno.h>
#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stack.h"

#define EXPRESSION_NAME_MAX 64
#define EXPRESSION_TEXT_MAX 256

enum expression_error_code {
    EXPRESSION_OK,
    EXPRESSION_SYNTAX_ERROR,
    EXPRESSION_PAREN_ERROR,
    EXPRESSION_UNKNOWN_VARIABLE,
    EXPRESSION_UNSET_VARIABLE,
    EXPRESSION_DIVISION_BY_ZERO,
    EXPRESSION_UNKNOWN_FUNCTION,
};

struct expression_error {
    enum expression_error_code code;
    size_t position;
};

struct expression_variable {
    char name[EXPRESSION_NAME_MAX];
    double value;
    bool assigned;
};

struct expression_function {
    char name[EXPRESSION_NAME_MAX];
    char parameter[EXPRESSION_NAME_MAX];
    char body[EXPRESSION_TEXT_MAX];
};

struct expression_context {
    struct vec variables;
    struct vec functions;
};

static inline void expression_init(struct expression_context *ctx)
{
    vec_init(&ctx->variables, struct expression_variable);
    vec_init(&ctx->functions, struct expression_function);
}

static inline void expression_destroy(struct expression_context *ctx)
{
    vec_destroy(&ctx->variables);
    vec_destroy(&ctx->functions);
}

static inline void expression_error(struct expression_error *error,
                                    enum expression_error_code code,
                                    size_t position)
{
    if (error) {
        error->code = code;
        error->position = position;
    }
}

static inline bool expression_name_start(unsigned char c)
{
    return isalpha(c) || c == '_';
}

static inline bool expression_name_char(unsigned char c)
{
    return isalnum(c) || c == '_';
}

static inline size_t expression_skip_space(const char *text, size_t position)
{
    while (isspace((unsigned char)text[position])) ++position;
    return position;
}

static inline bool expression_read_name(const char *text, size_t *position,
                                        char name[EXPRESSION_NAME_MAX])
{
    size_t start = *position;
    if (!expression_name_start((unsigned char)text[start])) return false;
    size_t end = start + 1;
    while (expression_name_char((unsigned char)text[end])) ++end;
    size_t length = end - start;
    if (length >= EXPRESSION_NAME_MAX) return false;
    memcpy(name, text + start, length);
    name[length] = '\0';
    *position = end;
    return true;
}

static inline struct expression_variable *
expression_variable(struct expression_context *ctx, const char *name)
{
    for (size_t i = 0; i < ctx->variables.size; ++i) {
        struct expression_variable *v = vec_at(&ctx->variables, i);
        if (strcmp(v->name, name) == 0) {
            return v;
        }
    }
    return NULL;
}

static inline bool expression_set_variable(struct expression_context *ctx,
                                           const char *name,
                                           double value)
{
    if (!expression_name_start((unsigned char)name[0]) ||
        strlen(name) >= EXPRESSION_NAME_MAX) return false;
    for (const unsigned char *p = (const unsigned char *)name + 1; *p; ++p) {
        if (!expression_name_char(*p)) {
            return false;
        }
    }
    struct expression_variable *v = expression_variable(ctx, name);
    if (!v) {
        struct expression_variable item = { .value = value, .assigned = true };
        strcpy(item.name, name);
        if (!vec_push_back(&ctx->variables, &item)) {
            return false;
        }
    } else {
        v->value = value;
        v->assigned = true;
    }
    return true;
}

static inline bool expression_collect_variables(struct expression_context *ctx,
                                                const char *text,
                                                struct expression_error *error)
{
    VEC(variables, struct expression_variable);
    for (size_t i = 0; text[i];) {
        unsigned char c = (unsigned char)text[i];
        if (isspace(c)) {
            ++i;
        } else if (isdigit(c) || c == '.') {
            char *end;
            errno = 0;
            strtod(text + i, &end);
            if (end == text + i || errno == ERANGE) goto syntax_error;
            i = (size_t)(end - text);
        } else if (expression_name_start(c)) {
            char name[EXPRESSION_NAME_MAX];
            if (!expression_read_name(text, &i, name)) goto syntax_error;
            if (text[expression_skip_space(text, i)] == '(') continue;
            size_t index = 0;
            while (index < variables.size &&
                   strcmp(((struct expression_variable *)vec_at(&variables, index))->name,
                          name) != 0) ++index;
            if (index == variables.size) {
                struct expression_variable item = { .assigned = false };
                strcpy(item.name, name);
                if (!vec_push_back(&variables, &item)) goto syntax_error;
            }
        } else if (c == '<' || c == '>') {
            i += text[i + 1] == '=' ? 2 : 1;
        } else if ((c == '=' || c == '!') && text[i + 1] == '=') {
            i += 2;
        } else if (strchr("+-*/^()?:", c)) {
            ++i;
        } else {
            goto syntax_error;
        }
        continue;
syntax_error:
        expression_error(error, EXPRESSION_SYNTAX_ERROR, i);
        vec_destroy(&variables);
        return false;
    }
    vec_swap(&ctx->variables, &variables);
    vec_destroy(&variables);
    expression_error(error, EXPRESSION_OK, 0);
    return true;
}

static inline bool expression_define(struct expression_context *ctx,
                                      const char *text,
                                      struct expression_error *error)
{
    struct expression_function function;
    size_t position = expression_skip_space(text, 0);
    if (strncmp(text + position, "DEF", 3) != 0 ||
        expression_name_char((unsigned char)text[position + 3])) goto syntax_error;
    position = expression_skip_space(text, position + 3);
    if (!expression_read_name(text, &position, function.name)) goto syntax_error;
    position = expression_skip_space(text, position);
    if (text[position] != '(') goto syntax_error;
    position = expression_skip_space(text, position + 1);
    if (!expression_read_name(text, &position, function.parameter)) goto syntax_error;
    position = expression_skip_space(text, position);
    if (text[position] != ')') goto syntax_error;
    position = expression_skip_space(text, position + 1);
    if (text[position] != '=') goto syntax_error;
    position = expression_skip_space(text, position + 1);

    size_t end = strlen(text);
    while (end > position && isspace((unsigned char)text[end - 1])) --end;
    if (end > position && text[end - 1] == ';') --end;
    while (end > position && isspace((unsigned char)text[end - 1])) --end;
    size_t length = end - position;
    if (length == 0 || length >= sizeof(function.body)) goto syntax_error;
    memcpy(function.body, text + position, length);
    function.body[length] = '\0';

    struct expression_context body;
    expression_init(&body);
    if (!expression_collect_variables(&body, function.body, error)) {
        if (error) error->position += position;
        expression_destroy(&body);
        return false;
    }
    for (size_t i = 0; i < body.variables.size; ++i) {
        const struct expression_variable *variable = vec_at_const(&body.variables, i);
        if (strcmp(variable->name, function.parameter) != 0) {
            expression_error(error, EXPRESSION_UNKNOWN_VARIABLE, position);
            expression_destroy(&body);
            return false;
        }
    }
    expression_destroy(&body);
    if (!vec_push_back(&ctx->functions, &function)) goto syntax_error;
    expression_error(error, EXPRESSION_OK, 0);
    return true;

syntax_error:
    expression_error(error, EXPRESSION_SYNTAX_ERROR, position);
    return false;
}

static inline size_t expression_find_function(const struct expression_context *ctx,
                                              const char *name,
                                              size_t current_function)
{
    if (current_function != SIZE_MAX) {
        const struct expression_function *function =
            vec_at_const(&ctx->functions, current_function);
        if (strcmp(function->name, name) == 0) return current_function;
    }
    for (size_t i = ctx->functions.size; i-- > 0;) {
        const struct expression_function *function = vec_at_const(&ctx->functions, i);
        if (strcmp(function->name, name) == 0) return i;
    }
    return SIZE_MAX;
}

enum expression_operator_kind {
    EXPRESSION_OPEN,
    EXPRESSION_POSITIVE,
    EXPRESSION_NEGATIVE,
    EXPRESSION_ADD,
    EXPRESSION_SUBTRACT,
    EXPRESSION_MULTIPLY,
    EXPRESSION_DIVIDE,
    EXPRESSION_POWER,
    EXPRESSION_LESS,
    EXPRESSION_LESS_EQUAL,
    EXPRESSION_GREATER,
    EXPRESSION_GREATER_EQUAL,
    EXPRESSION_EQUAL,
    EXPRESSION_NOT_EQUAL,
    EXPRESSION_CONDITIONAL,
};

/** @brief An operator and its byte offset in the expression. */
struct expression_operator {
    enum expression_operator_kind kind;
    size_t position;
    size_t function_index;
    size_t branch_end;
    size_t resume;
    size_t previous_branch;
};

static inline int expression_precedence(enum expression_operator_kind kind)
{
    switch (kind) {
    case EXPRESSION_POWER: return 7;
    case EXPRESSION_POSITIVE: case EXPRESSION_NEGATIVE: return 6;
    case EXPRESSION_MULTIPLY: case EXPRESSION_DIVIDE: return 5;
    case EXPRESSION_ADD: case EXPRESSION_SUBTRACT: return 4;
    case EXPRESSION_LESS: case EXPRESSION_LESS_EQUAL:
    case EXPRESSION_GREATER: case EXPRESSION_GREATER_EQUAL: return 3;
    case EXPRESSION_EQUAL: case EXPRESSION_NOT_EQUAL: return 2;
    default: return 0;
    }
}

/** @brief Applies the top operator to the value stack. */
static inline bool expression_apply(stack *values, stack *operators,
                                     struct expression_error *error)
{
    struct expression_operator op = *stack_top_as(operators, struct expression_operator);
    stack_pop(operators);
    bool unary = op.kind == EXPRESSION_POSITIVE || op.kind == EXPRESSION_NEGATIVE;
    if (stack_size(values) < (unary ? 1u : 2u)) {
        expression_error(error, EXPRESSION_SYNTAX_ERROR, op.position);
        return false;
    }
    if (unary) {
        if (op.kind == EXPRESSION_NEGATIVE) *stack_top_as(values, double) *= -1;
        return true;
    }
    double rhs = *stack_top_as(values, double);
    stack_pop(values);
    double *lhs = stack_top_as(values, double);
    switch (op.kind) {
    case EXPRESSION_ADD: *lhs += rhs; break;
    case EXPRESSION_SUBTRACT: *lhs -= rhs; break;
    case EXPRESSION_MULTIPLY: *lhs *= rhs; break;
    case EXPRESSION_DIVIDE:
        if (rhs == 0.0) {
            expression_error(error, EXPRESSION_DIVISION_BY_ZERO, op.position);
            return false;
        }
        *lhs /= rhs;
        break;
    case EXPRESSION_POWER: *lhs = pow(*lhs, rhs); break;
    case EXPRESSION_LESS: *lhs = *lhs < rhs; break;
    case EXPRESSION_LESS_EQUAL: *lhs = *lhs <= rhs; break;
    case EXPRESSION_GREATER: *lhs = *lhs > rhs; break;
    case EXPRESSION_GREATER_EQUAL: *lhs = *lhs >= rhs; break;
    case EXPRESSION_EQUAL: *lhs = *lhs == rhs; break;
    case EXPRESSION_NOT_EQUAL: *lhs = *lhs != rhs; break;
    default: break;
    }
    return true;
}

/* Find the matching colon and the end of the false branch without executing it. */
static inline bool expression_branch_bounds(const char *text, size_t question,
                                             size_t *colon, size_t *end,
                                             struct expression_error *error)
{
    size_t parentheses = 0;
    size_t questions = 0;
    size_t position = question + 1;
    *colon = SIZE_MAX;
    for (; text[position]; ++position) {
        char c = text[position];
        if (c == '(') {
            ++parentheses;
        } else if (c == ')') {
            if (parentheses == 0) break;
            --parentheses;
        } else if (parentheses == 0 && c == '?') {
            ++questions;
        } else if (parentheses == 0 && c == ':') {
            if (questions != 0) --questions;
            else if (*colon == SIZE_MAX) *colon = position;
            else break;
        }
    }
    *end = position;
    if (parentheses != 0) {
        expression_error(error, EXPRESSION_PAREN_ERROR, position);
        return false;
    }
    if (*colon == SIZE_MAX || questions != 0 ||
        expression_skip_space(text, question + 1) == *colon ||
        expression_skip_space(text, *colon + 1) == *end) {
        expression_error(error, EXPRESSION_SYNTAX_ERROR, question);
        return false;
    }
    return true;
}

/**
 * @brief Evaluates an expression using value and operator stacks.
 * @param ctx The variable table used for name lookup.
 * @param text The null-terminated expression.
 * @param result Receives the value on success; unchanged on failure.
 * @param error Receives the error code and byte offset, or may be null.
 * @return `true` on success; otherwise `false`.
 * @pre `ctx` is initialized; `text` and `result` are valid pointers.
 */
static inline bool __expression_eval(const struct expression_context *ctx,
                                     const char *text,
                                     size_t current_function,
                                     double argument,
                                     double *result,
                                     struct expression_error *error)
{
    STACK(values, double);
    STACK(operators, struct expression_operator);
    bool expect_operand = true;
    bool success = false;
    size_t position = 0;
    size_t branch = SIZE_MAX;

    for (;;) {
        position = expression_skip_space(text, position);
        if (branch != SIZE_MAX) {
            struct expression_operator boundary =
                vec_at_as(&operators.container, struct expression_operator, branch);
            if (position == boundary.branch_end) {
                if (expect_operand) goto syntax_error;
                while (stack_size(&operators) > branch + 1) {
                    if (!expression_apply(&values, &operators, error)) goto done;
                }
                stack_pop(&operators);
                branch = boundary.previous_branch;
                position = boundary.resume;
                continue;
            }
        }
        unsigned char c = (unsigned char)text[position];
        if (c == '\0') break;
        if (isdigit(c) || c == '.' || expression_name_start(c)) {
            if (!expect_operand) goto syntax_error;
            double value;
            if (expression_name_start(c)) {
                size_t start = position;
                char name[EXPRESSION_NAME_MAX];
                if (!expression_read_name(text, &position, name)) goto syntax_error;
                size_t next = expression_skip_space(text, position);
                if (text[next] == '(') {
                    size_t index = expression_find_function(ctx, name, current_function);
                    if (index == SIZE_MAX) {
                        expression_error(error, EXPRESSION_UNKNOWN_FUNCTION, start);
                        goto done;
                    }
                    struct expression_operator call = {
                        .kind = EXPRESSION_OPEN,
                        .position = next,
                        .function_index = index,
                    };
                    if (!stack_push(&operators, &call)) goto syntax_error;
                    position = next + 1;
                    continue;
                }
                if (current_function != SIZE_MAX) {
                    const struct expression_function *function =
                        vec_at_const(&ctx->functions, current_function);
                    if (strcmp(function->parameter, name) != 0) {
                        expression_error(error, EXPRESSION_UNKNOWN_VARIABLE, start);
                        goto done;
                    }
                    value = argument;
                } else {
                    const struct expression_variable *variable = NULL;
                    for (size_t i = 0; i < ctx->variables.size; ++i) {
                        const struct expression_variable *v = vec_at_const(&ctx->variables, i);
                        if (strcmp(v->name, name) == 0) {
                            variable = v;
                            break;
                        }
                    }
                    if (!variable || !variable->assigned) {
                        expression_error(error, variable ? EXPRESSION_UNSET_VARIABLE
                                                         : EXPRESSION_UNKNOWN_VARIABLE, start);
                        goto done;
                    }
                    value = variable->value;
                }
            } else {
                char *end;
                errno = 0;
                value = strtod(text + position, &end);
                if (end == text + position || errno == ERANGE) goto syntax_error;
                position = (size_t)(end - text);
            }
            if (!stack_push(&values, &value)) goto syntax_error;
            expect_operand = false;
            continue;
        }
        struct expression_operator op = {
            .position = position,
            .function_index = SIZE_MAX,
        };
        size_t length = 1;
        if (c == '(') {
            if (!expect_operand) goto syntax_error;
            op.kind = EXPRESSION_OPEN;
        } else if (c == ')') {
            if (expect_operand) goto syntax_error;
            while (!stack_empty(&operators) &&
                   stack_top_as(&operators, struct expression_operator)->kind != EXPRESSION_OPEN) {
                if (!expression_apply(&values, &operators, error)) goto done;
            }
            if (stack_empty(&operators)) goto paren_error;
            struct expression_operator open =
                *stack_top_as(&operators, struct expression_operator);
            stack_pop(&operators);
            if (open.function_index != SIZE_MAX) {
                const struct expression_function *function =
                    vec_at_const(&ctx->functions, open.function_index);
                double value;
                if (!__expression_eval(ctx, function->body, open.function_index,
                                       *stack_top_as(&values, double), &value, error)) goto done;
                *stack_top_as(&values, double) = value;
            }
            ++position;
            continue;
        } else if (c == '?') {
            if (expect_operand) goto syntax_error;
            while (!stack_empty(&operators) &&
                   expression_precedence(stack_top_as(&operators,
                       struct expression_operator)->kind) != 0) {
                if (!expression_apply(&values, &operators, error)) goto done;
            }
            size_t colon, end;
            if (!expression_branch_bounds(text, position, &colon, &end, error)) goto done;
            bool condition = *stack_top_as(&values, double) != 0.0;
            stack_pop(&values);
            op.kind = EXPRESSION_CONDITIONAL;
            op.branch_end = condition ? colon : end;
            op.resume = end;
            op.previous_branch = branch;
            size_t index = stack_size(&operators);
            if (!stack_push(&operators, &op)) goto syntax_error;
            branch = index;
            position = condition ? position + 1 : colon + 1;
            expect_operand = true;
            continue;
        } else {
            switch (c) {
            case '+': op.kind = EXPRESSION_ADD; break;
            case '-': op.kind = EXPRESSION_SUBTRACT; break;
            case '*': op.kind = EXPRESSION_MULTIPLY; break;
            case '/': op.kind = EXPRESSION_DIVIDE; break;
            case '^': op.kind = EXPRESSION_POWER; break;
            case '<':
                op.kind = text[position + 1] == '=' ? EXPRESSION_LESS_EQUAL : EXPRESSION_LESS;
                length = text[position + 1] == '=' ? 2 : 1;
                break;
            case '>':
                op.kind = text[position + 1] == '=' ? EXPRESSION_GREATER_EQUAL : EXPRESSION_GREATER;
                length = text[position + 1] == '=' ? 2 : 1;
                break;
            case '=': case '!':
                if (text[position + 1] != '=') goto syntax_error;
                op.kind = c == '=' ? EXPRESSION_EQUAL : EXPRESSION_NOT_EQUAL;
                length = 2;
                break;
            default: goto syntax_error;
            }
            if (expect_operand) {
                if (c != '+' && c != '-') goto syntax_error;
                op.kind = c == '+' ? EXPRESSION_POSITIVE : EXPRESSION_NEGATIVE;
            } else {
                while (!stack_empty(&operators)) {
                    int top = expression_precedence(stack_top_as(&operators,
                        struct expression_operator)->kind);
                    int current = expression_precedence(op.kind);
                    if (top < current || (top == current && op.kind == EXPRESSION_POWER)) break;
                    if (!expression_apply(&values, &operators, error)) goto done;
                }
            }
        }
        if (!stack_push(&operators, &op)) goto syntax_error;
        expect_operand = true;
        position += length;
    }
    if (expect_operand) goto syntax_error;
    while (!stack_empty(&operators)) {
        struct expression_operator op = *stack_top_as(&operators, struct expression_operator);
        if (op.kind == EXPRESSION_OPEN) {
            position = op.position;
            goto paren_error;
        }
        if (!expression_apply(&values, &operators, error)) goto done;
    }
    if (stack_size(&values) != 1) goto syntax_error;
    *result = *stack_top_as(&values, double);
    expression_error(error, EXPRESSION_OK, 0);
    success = true;
    goto done;

syntax_error:
    expression_error(error, EXPRESSION_SYNTAX_ERROR, position);
    goto done;
paren_error:
    expression_error(error, EXPRESSION_PAREN_ERROR, position);
done:
    stack_destroy(&values);
    stack_destroy(&operators);
    return success;
}

static inline bool expression_eval(const struct expression_context *ctx,
                                   const char *text,
                                   double *result,
                                   struct expression_error *error)
{
    return __expression_eval(ctx, text, SIZE_MAX, 0.0, result, error);
}

static inline bool expression_call(const struct expression_context *ctx,
                                   size_t function_index, double argument,
                                   double *result,
                                   struct expression_error *error)
{
    const struct expression_function *function =
        vec_at_const(&ctx->functions, function_index);
    return __expression_eval(ctx, function->body, function_index, argument, result, error);
}

#endif /* EXPRESSION_H */
