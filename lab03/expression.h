/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "stack.h"

#define EXPRESSION_NAME_MAX 64

enum expression_error_code {
    EXPRESSION_OK,
    EXPRESSION_SYNTAX_ERROR,
    EXPRESSION_PAREN_ERROR,
    EXPRESSION_UNKNOWN_VARIABLE,
    EXPRESSION_UNSET_VARIABLE,
    EXPRESSION_DIVISION_BY_ZERO,
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

struct expression_context {
    struct vec variables;
};

static inline void expression_init(struct expression_context *ctx)
{
    vec_init(&ctx->variables, struct expression_variable);
}

static inline void expression_destroy(struct expression_context *ctx)
{
    vec_destroy(&ctx->variables);
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
    vec_clear(&ctx->variables);
    for (size_t i = 0; text[i];) {
        unsigned char c = (unsigned char)text[i];
        if (isspace(c)) {
            ++i;
        } else if (expression_name_start(c)) {
            size_t start = i++;
            while (expression_name_char((unsigned char)text[i])) {
                ++i;
            }
            size_t length = i - start;
            if (length >= EXPRESSION_NAME_MAX) {
                expression_error(error, EXPRESSION_SYNTAX_ERROR, start);
                return false;
            }
            char name[EXPRESSION_NAME_MAX];
            memcpy(name, text + start, length);
            name[length] = '\0';
            if (!expression_variable(ctx, name)) {
                struct expression_variable item = { .assigned = false };
                strcpy(item.name, name);
                if (!vec_push_back(&ctx->variables, &item)) {
                    expression_error(error, EXPRESSION_SYNTAX_ERROR, start);
                    return false;
                }
            }
        } else if (isdigit(c) || c == '.' || c == '+' || c == '-' ||
                   c == '*' || c == '/' || c == '(' || c == ')') {
            ++i;
        } else {
            expression_error(error, EXPRESSION_SYNTAX_ERROR, i);
            return false;
        }
    }
    expression_error(error, EXPRESSION_OK, 0);
    return true;
}

/** @brief An operator and its byte offset in the expression. */
struct expression_operator {
    char symbol;
    size_t position;
};

/** @brief Returns precedence; `p` and `n` denote unary plus and minus. */
static inline int expression_precedence(char symbol)
{
    switch (symbol) {
    case 'p': case 'n': return 3;
    case '*': case '/': return 2;
    case '+': case '-': return 1;
    default: return 0;
    }
}

/** @brief Applies the top operator to the value stack. */
static inline bool expression_apply(stack *values, stack *operators,
                                     struct expression_error *error)
{
    struct expression_operator op = *stack_top_as(operators, struct expression_operator);
    stack_pop(operators);
    bool unary = op.symbol == 'p' || op.symbol == 'n';
    if (stack_size(values) < (unary ? 1u : 2u)) {
        expression_error(error, EXPRESSION_SYNTAX_ERROR, op.position);
        return false;
    }
    if (unary) {
        if (op.symbol == 'n') *stack_top_as(values, double) *= -1;
        return true;
    }
    double rhs = *stack_top_as(values, double);
    stack_pop(values);
    double *lhs = stack_top_as(values, double);
    switch (op.symbol) {
    case '+': *lhs += rhs; break;
    case '-': *lhs -= rhs; break;
    case '*': *lhs *= rhs; break;
    case '/':
        if (rhs == 0.0) {
            expression_error(error, EXPRESSION_DIVISION_BY_ZERO, op.position);
            return false;
        }
        *lhs /= rhs;
        break;
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
static inline bool expression_eval(const struct expression_context *ctx,
                                   const char *text,
                                   double *result,
                                   struct expression_error *error)
{
    STACK(values, double);
    STACK(operators, struct expression_operator);
    bool expect_operand = true;
    bool success = false;
    size_t position = 0;

    while (text[position]) {
        unsigned char c = (unsigned char)text[position];
        if (isspace(c)) {
            ++position;
            continue;
        }
        if (isdigit(c) || c == '.' || expression_name_start(c)) {
            if (!expect_operand) goto syntax_error;
            double value;
            if (expression_name_start(c)) {
                size_t start = position++;
                while (expression_name_char((unsigned char)text[position])) ++position;
                size_t length = position - start;
                if (length >= EXPRESSION_NAME_MAX) {
                    position = start;
                    goto syntax_error;
                }
                const struct expression_variable *variable = NULL;
                for (size_t i = 0; i < ctx->variables.size; ++i) {
                    const struct expression_variable *v = vec_at_const(&ctx->variables, i);
                    if (strlen(v->name) == length &&
                        strncmp(v->name, text + start, length) == 0) {
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
        struct expression_operator op = { .symbol = (char)c, .position = position };
        if (c == '(') {
            if (!expect_operand) goto syntax_error;
        } else if (c == ')') {
            if (expect_operand) goto syntax_error;
            while (!stack_empty(&operators) &&
                   stack_top_as(&operators, struct expression_operator)->symbol != '(') {
                if (!expression_apply(&values, &operators, error)) goto done;
            }
            if (stack_empty(&operators)) goto paren_error;
            stack_pop(&operators);
            ++position;
            continue;
        } else if (c == '+' || c == '-' || c == '*' || c == '/') {
            if (expect_operand) {
                if (c != '+' && c != '-') goto syntax_error;
                op.symbol = c == '+' ? 'p' : 'n';
            } else {
                while (!stack_empty(&operators) &&
                       expression_precedence(stack_top_as(&operators,
                           struct expression_operator)->symbol) >= expression_precedence(op.symbol)) {
                    if (!expression_apply(&values, &operators, error)) goto done;
                }
            }
        } else {
            goto syntax_error;
        }
        if (!stack_push(&operators, &op)) goto syntax_error;
        expect_operand = true;
        ++position;
    }
    if (expect_operand) goto syntax_error;
    while (!stack_empty(&operators)) {
        struct expression_operator op = *stack_top_as(&operators, struct expression_operator);
        if (op.symbol == '(') {
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

#endif /* EXPRESSION_H */
