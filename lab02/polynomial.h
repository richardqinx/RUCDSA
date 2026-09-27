/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#ifndef POLYNOMIAL_H
#define POLYNOMIAL_H

#include <stddef.h>
#include <stdbool.h>
#include <limits.h>
#include <math.h>
#include <stdlib.h>

#include "vec.h"
#include "list.h"

/*
 * Polynomial term:
 *
 *     coef * x^exp
 *
 * exp >= 0
 */
struct poly_term {
    double coef;
    unsigned int exp;
};

/*
 * ============================================================
 * Sequential representation
 * ============================================================
 *
 * Terms are stored in descending order of exponent:
 *
 *     5x^4 + 3x^2 - 7
 *
 *     [ {5,4}, {3,2}, {-7,0} ]
 *
 * Invariants:
 *
 *     terms.elem_size == sizeof(struct poly_term)
 *     terms are ordered by exp descending
 *     duplicate exponents do not exist
 *     zero-coefficient terms do not exist
 */

struct poly_vec {
    struct vec terms;
};

/*
 * ============================================================
 * Linked representation
 * ============================================================
 */

struct poly_node {
    struct poly_term term;
    struct list_head link;
};

struct poly_list {
    struct list_head head;
    size_t size;
};

/*
 * ============================================================
 * Initialization / destruction
 * ============================================================
 */

static inline void poly_vec_init(struct poly_vec *poly)
{
    vec_init(&poly->terms, struct poly_term);
}

static inline void poly_vec_clear(struct poly_vec *poly)
{
    vec_clear(&poly->terms);
}

static inline void poly_vec_destroy(struct poly_vec *poly)
{
    vec_destroy(&poly->terms);
}

static inline void poly_list_init(struct poly_list *poly)
{
    INIT_LIST_HEAD(&poly->head);
    poly->size = 0;
}

static inline void poly_list_clear(struct poly_list *poly)
{
    struct list_head *pos = poly->head.next;

    while (pos != &poly->head) {
        struct list_head *next = pos->next;
        struct poly_node *node = list_entry(pos, struct poly_node, link);
        list_del(pos);
        free(node);
        pos = next;
    }

    INIT_LIST_HEAD(&poly->head);
    poly->size = 0;
}

static inline void poly_list_destroy(struct poly_list *poly)
{
    poly_list_clear(poly);
}

static inline void __poly_vec_replace(struct poly_vec *dst,
                                      struct poly_vec *src)
{
    struct vec old = dst->terms;
    dst->terms = src->terms;
    src->terms = old;
    vec_destroy(&src->terms);
}

static inline void __poly_list_replace(struct poly_list *dst,
                                       struct poly_list *src)
{
    poly_list_clear(dst);
    if (src->size == 0) {
        return;
    }

    dst->head.next = src->head.next;
    dst->head.prev = src->head.prev;
    dst->head.next->prev = &dst->head;
    dst->head.prev->next = &dst->head;
    dst->size = src->size;

    INIT_LIST_HEAD(&src->head);
    src->size = 0;
}

/*
 * ============================================================
 * Basic properties
 * ============================================================
 */

static inline bool poly_vec_empty(const struct poly_vec *poly)
{
    return vec_empty(&poly->terms);
}

static inline size_t poly_vec_size(const struct poly_vec *poly)
{
    return vec_size(&poly->terms);
}


static inline bool poly_list_empty(const struct poly_list *poly)
{
    return list_empty(&poly->head);
}

static inline size_t poly_list_size(const struct poly_list *poly)
{
    return poly->size;
}

/*
 * ============================================================
 * Term access
 * ============================================================
 *
 * Sequential representation supports indexed access.
 */

static inline struct poly_term *
poly_vec_at(struct poly_vec *poly, size_t index)
{
    return vec_at(&poly->terms, index);
}

static inline const struct poly_term *
poly_vec_at_const(const struct poly_vec *poly, size_t index)
{
    return vec_at_const(&poly->terms, index);
}


/*
 * ============================================================
 * Term modification
 * ============================================================
 *
 * poly_*_set_term():
 *
 *     Sets the coefficient for x^exp.
 *
 *     Existing exponent:
 *         replace coefficient
 *
 *     Missing exponent:
 *         insert at the proper position
 *
 *     coef == 0:
 *         remove that exponent
 *
 *
 * poly_*_add_term():
 *
 *     Adds coef*x^exp to the polynomial.
 *
 *     Existing exponent:
 *         old_coef += coef
 *
 *     Result coefficient == 0:
 *         remove that term
 *
 * These two operations preserve all polynomial invariants.
 */

static inline size_t __poly_vec_find(const struct poly_vec *poly,
                                     unsigned int exp,
                                     bool *found)
{
    size_t i;

    for (i = 0; i < vec_size(&poly->terms); ++i) {
        const struct poly_term *term = vec_at_const(&poly->terms, i);
        if (term->exp == exp) {
            *found = true;
            return i;
        }
        if (term->exp < exp) {
            break;
        }
    }

    *found = false;
    return i;
}

static inline bool poly_vec_remove_term(struct poly_vec *poly,
                                        unsigned int exp);

static inline bool poly_vec_set_term(struct poly_vec *poly,
                                     double coef,
                                     unsigned int exp)
{
    if (coef == 0.0) {
        poly_vec_remove_term(poly, exp);
        return true;
    }

    bool found;
    size_t index = __poly_vec_find(poly, exp, &found);
    if (found) {
        poly_vec_at(poly, index)->coef = coef;
        return true;
    }

    struct poly_term term = { .coef = coef, .exp = exp };
    return vec_insert(&poly->terms, index, &term);
}

static inline bool poly_vec_add_term(struct poly_vec *poly,
                                     double coef,
                                     unsigned int exp)
{
    if (coef == 0.0) {
        return true;
    }

    bool found;
    size_t index = __poly_vec_find(poly, exp, &found);
    if (found) {
        struct poly_term *term = poly_vec_at(poly, index);
        double result = term->coef + coef;
        if (result == 0.0) {
            vec_erase(&poly->terms, index);
        } else {
            term->coef = result;
        }
        return true;
    }

    struct poly_term term = { .coef = coef, .exp = exp };
    return vec_insert(&poly->terms, index, &term);
}

static inline bool poly_vec_remove_term(struct poly_vec *poly,
                                        unsigned int exp)
{
    bool found;
    size_t index = __poly_vec_find(poly, exp, &found);
    if (!found) {
        return false;
    }

    vec_erase(&poly->terms, index);
    return true;
}

static inline bool poly_vec_get_term(const struct poly_vec *poly,
                                     unsigned int exp,
                                     double *coef)
{
    bool found;
    size_t index = __poly_vec_find(poly, exp, &found);
    if (!found) {
        return false;
    }

    *coef = poly_vec_at_const(poly, index)->coef;
    return true;
}

static inline bool poly_list_remove_term(struct poly_list *poly,
                                         unsigned int exp);

static inline bool poly_list_set_term(struct poly_list *poly,
                                      double coef,
                                      unsigned int exp)
{
    if (coef == 0.0) {
        poly_list_remove_term(poly, exp);
        return true;
    }

    struct list_head *pos = poly->head.next;
    while (pos != &poly->head) {
        struct poly_node *node = list_entry(pos, struct poly_node, link);
        if (node->term.exp == exp) {
            node->term.coef = coef;
            return true;
        }
        if (node->term.exp < exp) {
            break;
        }
        pos = pos->next;
    }

    struct poly_node *node = malloc(sizeof(*node));
    if (!node) {
        return false;
    }
    node->term.coef = coef;
    node->term.exp = exp;
    __list_add(&node->link, pos->prev, pos);
    ++poly->size;
    return true;
}

static inline bool poly_list_add_term(struct poly_list *poly,
                                      double coef,
                                      unsigned int exp)
{
    if (coef == 0.0) {
        return true;
    }

    struct list_head *pos = poly->head.next;
    while (pos != &poly->head) {
        struct poly_node *node = list_entry(pos, struct poly_node, link);
        if (node->term.exp == exp) {
            double result = node->term.coef + coef;
            if (result == 0.0) {
                list_del(pos);
                free(node);
                --poly->size;
            } else {
                node->term.coef = result;
            }
            return true;
        }
        if (node->term.exp < exp) {
            break;
        }
        pos = pos->next;
    }

    struct poly_node *node = malloc(sizeof(*node));
    if (!node) {
        return false;
    }
    node->term.coef = coef;
    node->term.exp = exp;
    __list_add(&node->link, pos->prev, pos);
    ++poly->size;
    return true;
}

static inline bool poly_list_remove_term(struct poly_list *poly,
                                         unsigned int exp)
{
    struct list_head *pos = poly->head.next;
    while (pos != &poly->head) {
        struct poly_node *node = list_entry(pos, struct poly_node, link);
        if (node->term.exp == exp) {
            list_del(pos);
            free(node);
            --poly->size;
            return true;
        }
        if (node->term.exp < exp) {
            return false;
        }
        pos = pos->next;
    }
    return false;
}

static inline bool poly_list_get_term(const struct poly_list *poly,
                                      unsigned int exp,
                                      double *coef)
{
    const struct list_head *pos = poly->head.next;
    while (pos != &poly->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        if (node->term.exp == exp) {
            *coef = node->term.coef;
            return true;
        }
        if (node->term.exp < exp) {
            return false;
        }
        pos = pos->next;
    }
    return false;
}


/*
 * ============================================================
 * Copy / conversion
 * ============================================================
 */

static inline bool poly_vec_copy(struct poly_vec *dst,
                                 const struct poly_vec *src)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);

    if (!vec_append(&tmp.terms, src->terms.data, src->terms.size)) {
        poly_vec_destroy(&tmp);
        return false;
    }

    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_copy(struct poly_list *dst,
                                  const struct poly_list *src)
{
    struct poly_list tmp;
    poly_list_init(&tmp);

    const struct list_head *pos = src->head.next;
    while (pos != &src->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        if (!poly_list_set_term(&tmp, node->term.coef, node->term.exp)) {
            poly_list_destroy(&tmp);
            return false;
        }
        pos = pos->next;
    }

    __poly_list_replace(dst, &tmp);
    return true;
}


/*
 * Conversion between the two representations.
 */
static inline bool poly_vec_from_list(struct poly_vec *dst,
                                      const struct poly_list *src)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);

    const struct list_head *pos = src->head.next;
    while (pos != &src->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        if (!poly_vec_add_term(&tmp, node->term.coef, node->term.exp)) {
            poly_vec_destroy(&tmp);
            return false;
        }
        pos = pos->next;
    }

    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_from_vec(struct poly_list *dst,
                                      const struct poly_vec *src)
{
    struct poly_list tmp;
    poly_list_init(&tmp);

    for (size_t i = 0; i < src->terms.size; ++i) {
        const struct poly_term *term =
            vec_at_const(&src->terms, i);
        if (!poly_list_add_term(&tmp, term->coef, term->exp)) {
            poly_list_destroy(&tmp);
            return false;
        }
    }

    __poly_list_replace(dst, &tmp);
    return true;
}


/*
 * ============================================================
 * Arithmetic
 * ============================================================
 *
 * dst may alias lhs or rhs unless otherwise documented.
 */

static inline bool __poly_vec_add_scaled(struct poly_vec *dst,
                                         const struct poly_vec *src,
                                         double scale)
{
    for (size_t i = 0; i < src->terms.size; ++i) {
        const struct poly_term *term =
            vec_at_const(&src->terms, i);
        if (!poly_vec_add_term(dst, term->coef * scale, term->exp)) {
            return false;
        }
    }
    return true;
}

static inline bool __poly_list_add_scaled(struct poly_list *dst,
                                          const struct poly_list *src,
                                          double scale)
{
    const struct list_head *pos = src->head.next;
    while (pos != &src->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        if (!poly_list_add_term(dst, node->term.coef * scale,
                                node->term.exp)) {
            return false;
        }
        pos = pos->next;
    }
    return true;
}

static inline bool poly_vec_add(struct poly_vec *dst,
                                const struct poly_vec *lhs,
                                const struct poly_vec *rhs)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);
    if (!__poly_vec_add_scaled(&tmp, lhs, 1.0) ||
        !__poly_vec_add_scaled(&tmp, rhs, 1.0)) {
        poly_vec_destroy(&tmp);
        return false;
    }
    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_vec_sub(struct poly_vec *dst,
                                const struct poly_vec *lhs,
                                const struct poly_vec *rhs)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);
    if (!__poly_vec_add_scaled(&tmp, lhs, 1.0) ||
        !__poly_vec_add_scaled(&tmp, rhs, -1.0)) {
        poly_vec_destroy(&tmp);
        return false;
    }
    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_add(struct poly_list *dst,
                                 const struct poly_list *lhs,
                                 const struct poly_list *rhs)
{
    struct poly_list tmp;
    poly_list_init(&tmp);
    if (!__poly_list_add_scaled(&tmp, lhs, 1.0) ||
        !__poly_list_add_scaled(&tmp, rhs, 1.0)) {
        poly_list_destroy(&tmp);
        return false;
    }
    __poly_list_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_sub(struct poly_list *dst,
                                 const struct poly_list *lhs,
                                 const struct poly_list *rhs)
{
    struct poly_list tmp;
    poly_list_init(&tmp);
    if (!__poly_list_add_scaled(&tmp, lhs, 1.0) ||
        !__poly_list_add_scaled(&tmp, rhs, -1.0)) {
        poly_list_destroy(&tmp);
        return false;
    }
    __poly_list_replace(dst, &tmp);
    return true;
}


/*
 * ============================================================
 * Extended operations
 * ============================================================
 */

static inline bool poly_vec_mul(struct poly_vec *dst,
                                const struct poly_vec *lhs,
                                const struct poly_vec *rhs)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);

    for (size_t i = 0; i < lhs->terms.size; ++i) {
        const struct poly_term *left =
            vec_at_const(&lhs->terms, i);
        for (size_t j = 0; j < rhs->terms.size; ++j) {
            const struct poly_term *right =
                vec_at_const(&rhs->terms, j);
            if (left->exp > UINT_MAX - right->exp ||
                !poly_vec_add_term(&tmp,
                                   left->coef * right->coef,
                                   left->exp + right->exp)) {
                poly_vec_destroy(&tmp);
                return false;
            }
        }
    }

    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_mul(struct poly_list *dst,
                                 const struct poly_list *lhs,
                                 const struct poly_list *rhs)
{
    struct poly_list tmp;
    poly_list_init(&tmp);

    const struct list_head *left_pos = lhs->head.next;
    while (left_pos != &lhs->head) {
        const struct poly_node *left =
            list_entry(left_pos, const struct poly_node, link);
        const struct list_head *right_pos = rhs->head.next;
        while (right_pos != &rhs->head) {
            const struct poly_node *right =
                list_entry(right_pos, const struct poly_node, link);
            if (left->term.exp > UINT_MAX - right->term.exp ||
                !poly_list_add_term(&tmp,
                                    left->term.coef * right->term.coef,
                                    left->term.exp + right->term.exp)) {
                poly_list_destroy(&tmp);
                return false;
            }
            right_pos = right_pos->next;
        }
        left_pos = left_pos->next;
    }

    __poly_list_replace(dst, &tmp);
    return true;
}


/*
 * Differentiate polynomial 'order' times.
 *
 * order == 0:
 *     dst = src
 */
static inline bool poly_vec_derivative(struct poly_vec *dst,
                                       const struct poly_vec *src,
                                       unsigned int order)
{
    struct poly_vec tmp;
    poly_vec_init(&tmp);

    for (size_t i = 0; i < src->terms.size; ++i) {
        const struct poly_term *term =
            vec_at_const(&src->terms, i);
        if (term->exp < order) {
            continue;
        }

        double coef = term->coef;
        unsigned int exp = term->exp;
        for (unsigned int j = 0; j < order; ++j) {
            coef *= exp;
            --exp;
        }
        if (!poly_vec_add_term(&tmp, coef, exp)) {
            poly_vec_destroy(&tmp);
            return false;
        }
    }

    __poly_vec_replace(dst, &tmp);
    return true;
}

static inline bool poly_list_derivative(struct poly_list *dst,
                                        const struct poly_list *src,
                                        unsigned int order)
{
    struct poly_list tmp;
    poly_list_init(&tmp);

    const struct list_head *pos = src->head.next;
    while (pos != &src->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        if (node->term.exp >= order) {
            double coef = node->term.coef;
            unsigned int exp = node->term.exp;
            for (unsigned int i = 0; i < order; ++i) {
                coef *= exp;
                --exp;
            }
            if (!poly_list_add_term(&tmp, coef, exp)) {
                poly_list_destroy(&tmp);
                return false;
            }
        }
        pos = pos->next;
    }

    __poly_list_replace(dst, &tmp);
    return true;
}


/*
 * ============================================================
 * Evaluation
 * ============================================================
 */

static inline double poly_vec_eval(const struct poly_vec *poly, double x)
{
    double result = 0.0;

    for (size_t i = 0; i < poly->terms.size; ++i) {
        const struct poly_term *term =
            vec_at_const(&poly->terms, i);
        result += term->coef * pow(x, term->exp);
    }
    return result;
}

static inline double poly_list_eval(const struct poly_list *poly, double x)
{
    double result = 0.0;
    const struct list_head *pos = poly->head.next;

    while (pos != &poly->head) {
        const struct poly_node *node =
            list_entry(pos, const struct poly_node, link);
        result += node->term.coef * pow(x, node->term.exp);
        pos = pos->next;
    }
    return result;
}


/*
 * ============================================================
 * Comparison
 * ============================================================
 *
 * eps controls floating-point coefficient comparison.
 */

static inline bool poly_vec_equal(const struct poly_vec *lhs,
                                  const struct poly_vec *rhs,
                                  double eps)
{
    if (lhs->terms.size != rhs->terms.size) {
        return false;
    }

    for (size_t i = 0; i < lhs->terms.size; ++i) {
        const struct poly_term *left =
            vec_at_const(&lhs->terms, i);
        const struct poly_term *right =
            vec_at_const(&rhs->terms, i);
        if (left->exp != right->exp ||
            !(fabs(left->coef - right->coef) <= eps)) {
            return false;
        }
    }
    return true;
}

static inline bool poly_list_equal(const struct poly_list *lhs,
                                   const struct poly_list *rhs,
                                   double eps)
{
    if (lhs->size != rhs->size) {
        return false;
    }

    const struct list_head *left_pos = lhs->head.next;
    const struct list_head *right_pos = rhs->head.next;
    while (left_pos != &lhs->head) {
        const struct poly_node *left =
            list_entry(left_pos, const struct poly_node, link);
        const struct poly_node *right =
            list_entry(right_pos, const struct poly_node, link);
        if (left->term.exp != right->term.exp ||
            !(fabs(left->term.coef - right->term.coef) <= eps)) {
            return false;
        }
        left_pos = left_pos->next;
        right_pos = right_pos->next;
    }
    return true;
}


/*
 * ============================================================
 * Iteration: sequential representation
 * ============================================================
 */

#define poly_vec_for_each(term, poly)                              \
    for (size_t __poly_i = 0;                                     \
         __poly_i < poly_vec_size(poly) &&                         \
         (((term) = poly_vec_at((poly), __poly_i)), true);         \
         ++__poly_i)

#define poly_vec_for_each_const(term, poly)                        \
    for (size_t __poly_i = 0;                                     \
         __poly_i < poly_vec_size(poly) &&                         \
         (((term) = poly_vec_at_const((poly), __poly_i)), true);   \
         ++__poly_i)


/*
 * ============================================================
 * Iteration: linked representation
 * ============================================================
 */

#define poly_list_for_each(node, poly) \
    list_for_each_entry((node), &(poly)->head, link)

#define poly_list_for_each_reverse(node, poly) \
    list_for_each_entry_reverse((node), &(poly)->head, link)

#define poly_list_for_each_safe(node, next, poly) \
    list_for_each_entry_safe((node), (next), &(poly)->head, link)


#endif /* POLYNOMIAL_H */
