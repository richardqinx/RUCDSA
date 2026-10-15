/* SPDX-License-Identifier: GPL-2.0-or-later */
#ifndef EXPRESSION_H
#define EXPRESSION_H

#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "vec.h"

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

struct expression_parser {
    const char *text;
    size_t position;
    const struct expression_context *ctx;
    struct expression_error *error;
};

static inline void expression_skip(struct expression_parser *p)
{
    while (isspace((unsigned char)p->text[p->position])) {
        ++p->position;
    }
}

static inline bool expression_parse_sum(struct expression_parser *, double *);

static inline bool expression_parse_primary(struct expression_parser *p,
                                            double *out)
{
    expression_skip(p);
    size_t start = p->position;
    char c = p->text[start];
    if (c == '(') {
        ++p->position;
        if (!expression_parse_sum(p, out)) return false;
        expression_skip(p);
        if (p->text[p->position] != ')') {
            expression_error(p->error, EXPRESSION_PAREN_ERROR, p->position);
            return false;
        }
        ++p->position;
        return true;
    }
    if (isdigit((unsigned char)c) || c == '.') {
        char *end;
        errno = 0;
        *out = strtod(p->text + start, &end);
        if (end == p->text + start || errno == ERANGE) {
            expression_error(p->error, EXPRESSION_SYNTAX_ERROR, start);
            return false;
        }
        p->position = (size_t)(end - p->text);
        return true;
    }
    if (expression_name_start((unsigned char)c)) {
        char name[EXPRESSION_NAME_MAX];
        size_t length;
        ++p->position;
        while (expression_name_char((unsigned char)p->text[p->position])) ++p->position;
        length = p->position - start;
        if (length >= sizeof(name)) {
            expression_error(p->error, EXPRESSION_SYNTAX_ERROR, start);
            return false;
        }
        memcpy(name, p->text + start, length);
        name[length] = '\0';
        const struct expression_variable *v =
            expression_variable((struct expression_context *)p->ctx, name);
        if (!v) expression_error(p->error, EXPRESSION_UNKNOWN_VARIABLE, start);
        else if (!v->assigned) expression_error(p->error, EXPRESSION_UNSET_VARIABLE, start);
        else *out = v->value;
        return v && v->assigned;
    }
    expression_error(p->error, EXPRESSION_SYNTAX_ERROR, start);
    return false;
}

static inline bool expression_parse_unary(struct expression_parser *p, double *out)
{
    expression_skip(p);
    char c = p->text[p->position];
    if (c == '+' || c == '-') {
        ++p->position;
        if (!expression_parse_unary(p, out)) return false;
        if (c == '-') *out = -*out;
        return true;
    }
    return expression_parse_primary(p, out);
}

static inline bool expression_parse_product(struct expression_parser *p, double *out)
{
    if (!expression_parse_unary(p, out)) return false;
    for (;;) {
        expression_skip(p);
        char op = p->text[p->position];
        if (op != '*' && op != '/') return true;
        ++p->position;
        double rhs;
        if (!expression_parse_unary(p, &rhs)) return false;
        if (op == '/') {
            if (rhs == 0.0) {
                expression_error(p->error, EXPRESSION_DIVISION_BY_ZERO, p->position);
                return false;
            }
            *out /= rhs;
        } else *out *= rhs;
    }
}

static inline bool expression_parse_sum(struct expression_parser *p, double *out)
{
    if (!expression_parse_product(p, out)) return false;
    for (;;) {
        expression_skip(p);
        char op = p->text[p->position];
        if (op != '+' && op != '-') return true;
        ++p->position;
        double rhs;
        if (!expression_parse_product(p, &rhs)) return false;
        if (op == '+') *out += rhs;
        else *out -= rhs;
    }
}

static inline bool expression_eval(const struct expression_context *ctx,
                                   const char *text,
                                   double *result,
                                   struct expression_error *error)
{
    struct expression_parser p = { text, 0, ctx, error };
    if (!expression_parse_sum(&p, result)) return false;
    expression_skip(&p);
    if (p.text[p.position]) {
        expression_error(error,
                         p.text[p.position] == ')' ? EXPRESSION_PAREN_ERROR
                                                    : EXPRESSION_SYNTAX_ERROR,
                         p.position);
        return false;
    }
    expression_error(error, EXPRESSION_OK, 0);
    return true;
}

#endif /* EXPRESSION_H */
