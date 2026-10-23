/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#ifndef VECTOR_MATH_H
#define VECTOR_MATH_H

#include <math.h>

#include "vec.h"

/* Real vectors use a double-typed struct vec. Destinations may alias inputs. */
static inline bool vector_add(struct vec *dst,
                              const struct vec *lhs,
                              const struct vec *rhs)
{
    if (lhs->size != rhs->size) return false;

    struct vec temporary;
    vec_init(&temporary, double);
    if (!vec_resize(&temporary, lhs->size)) return false;
    double *result = temporary.data;
    const double *left = lhs->data;
    const double *right = rhs->data;
    for (size_t i = 0; i < lhs->size; ++i) {
        result[i] = left[i] + right[i];
    }
    vec_swap(dst, &temporary);
    vec_destroy(&temporary);
    return true;
}

static inline bool vector_sub(struct vec *dst,
                              const struct vec *lhs,
                              const struct vec *rhs)
{
    if (lhs->size != rhs->size) return false;

    struct vec temporary;
    vec_init(&temporary, double);
    if (!vec_resize(&temporary, lhs->size)) return false;
    double *result = temporary.data;
    const double *left = lhs->data;
    const double *right = rhs->data;
    for (size_t i = 0; i < lhs->size; ++i) {
        result[i] = left[i] - right[i];
    }
    vec_swap(dst, &temporary);
    vec_destroy(&temporary);
    return true;
}

static inline bool vector_cosine(const struct vec *lhs,
                                 const struct vec *rhs,
                                 double *result)
{
    if (lhs->size != rhs->size) return false;

    double dot = 0.0;
    double norm_lhs = 0.0;
    double norm_rhs = 0.0;
    for (size_t i = 0; i < lhs->size; ++i) {
        double left = ((const double *)lhs->data)[i];
        double right = ((const double *)rhs->data)[i];
        dot += left * right;
        norm_lhs += left * left;
        norm_rhs += right * right;
    }
    if (norm_lhs == 0.0 || norm_rhs == 0.0) return false;
    *result = dot / (sqrt(norm_lhs) * sqrt(norm_rhs));
    return true;
}

#endif /* VECTOR_MATH_H */
