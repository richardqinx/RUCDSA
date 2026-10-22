/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#ifndef MATRIX_H
#define MATRIX_H

#include <float.h>
#include <math.h>

#include "vec.h"

/* Real elements in row-major order: elements[row * cols + col]. */
struct matrix {
    size_t rows;
    size_t cols;
    struct vec elements;
};

struct matrix_eigenvalue {
    double real;
    double imag;
};

static inline void matrix_init(struct matrix *matrix)
{
    matrix->rows = 0;
    matrix->cols = 0;
    vec_init(&matrix->elements, double);
}

static inline void matrix_destroy(struct matrix *matrix)
{
    vec_destroy(&matrix->elements);
    matrix->rows = 0;
    matrix->cols = 0;
}

/* Retains the leading elements of the flat storage; new elements are zeroed. */
static inline bool matrix_resize(struct matrix *matrix, size_t rows, size_t cols)
{
    if (rows == 0 || cols == 0 || rows > SIZE_MAX / cols) return false;
    if (!vec_resize_zero(&matrix->elements, rows * cols)) return false;
    matrix->rows = rows;
    matrix->cols = cols;
    return true;
}

/* Indices must be within the matrix dimensions. */
static inline double *matrix_at(struct matrix *matrix, size_t row, size_t col)
{
    return (double *)matrix->elements.data + row * matrix->cols + col;
}

static inline const double *matrix_at_const(const struct matrix *matrix,
                                            size_t row, size_t col)
{
    return (const double *)matrix->elements.data + row * matrix->cols + col;
}

static inline void __matrix_replace(struct matrix *dst, struct matrix *src)
{
    struct matrix old = *dst;
    *dst = *src;
    *src = old;
    matrix_destroy(src);
}

/* All matrix arguments must be initialized. Destinations may alias operands.
 * On failure, destinations and output values are unchanged.
 */
static inline bool matrix_assign(struct matrix *dst, size_t rows, size_t cols,
                                  const double *elements)
{
    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, rows, cols)) return false;
    memcpy(temporary.elements.data, elements, temporary.elements.size * sizeof(double));
    __matrix_replace(dst, &temporary);
    return true;
}

static inline bool matrix_copy(struct matrix *dst, const struct matrix *src)
{
    struct matrix temporary;
    matrix_init(&temporary);
    if (!vec_append(&temporary.elements, src->elements.data, src->elements.size)) {
        matrix_destroy(&temporary);
        return false;
    }
    temporary.rows = src->rows;
    temporary.cols = src->cols;
    __matrix_replace(dst, &temporary);
    return true;
}

static inline bool matrix_add(struct matrix *dst, const struct matrix *lhs,
                               const struct matrix *rhs)
{
    if (lhs->rows != rhs->rows || lhs->cols != rhs->cols) return false;
    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, lhs->rows, lhs->cols)) return false;
    const double *a = lhs->elements.data;
    const double *b = rhs->elements.data;
    double *result = temporary.elements.data;
    for (size_t i = 0; i < lhs->elements.size; ++i) result[i] = a[i] + b[i];
    __matrix_replace(dst, &temporary);
    return true;
}

static inline bool matrix_sub(struct matrix *dst, const struct matrix *lhs,
                               const struct matrix *rhs)
{
    if (lhs->rows != rhs->rows || lhs->cols != rhs->cols) return false;
    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, lhs->rows, lhs->cols)) return false;
    const double *a = lhs->elements.data;
    const double *b = rhs->elements.data;
    double *result = temporary.elements.data;
    for (size_t i = 0; i < lhs->elements.size; ++i) result[i] = a[i] - b[i];
    __matrix_replace(dst, &temporary);
    return true;
}

static inline bool matrix_mul(struct matrix *dst, const struct matrix *lhs,
                               const struct matrix *rhs)
{
    if (lhs->cols != rhs->rows) return false;
    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, lhs->rows, rhs->cols)) return false;
    const double *a = lhs->elements.data;
    const double *b = rhs->elements.data;
    double *result = temporary.elements.data;
    for (size_t i = 0; i < lhs->rows; ++i) {
        for (size_t k = 0; k < lhs->cols; ++k) {
            double value = a[i * lhs->cols + k];
            for (size_t j = 0; j < rhs->cols; ++j) {
                result[i * rhs->cols + j] += value * b[k * rhs->cols + j];
            }
        }
    }
    __matrix_replace(dst, &temporary);
    return true;
}

static inline bool matrix_transpose(struct matrix *dst, const struct matrix *src)
{
    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, src->cols, src->rows)) return false;
    const double *a = src->elements.data;
    double *result = temporary.elements.data;
    for (size_t i = 0; i < src->rows; ++i) {
        for (size_t j = 0; j < src->cols; ++j) {
            result[j * src->rows + i] = a[i * src->cols + j];
        }
    }
    __matrix_replace(dst, &temporary);
    return true;
}

/* Gauss-Jordan elimination with partial pivoting. A singular matrix has no
 * inverse. The destination is unchanged when the input is invalid, singular,
 * or allocation fails; the destination may alias the source.
 */
static inline bool matrix_inverse(struct matrix *dst, const struct matrix *src)
{
    if (src->rows == 0 || src->rows != src->cols) return false;
    size_t n = src->rows;
    if (n > SIZE_MAX / 2) return false;
    size_t width = n * 2;
    if (n > SIZE_MAX / width) return false;

    VEC(work, long double);
    if (!vec_resize(&work, n * width)) return false;
    long double *augmented = work.data;
    const double *source = src->elements.data;
    for (size_t row = 0; row < n; ++row) {
        for (size_t col = 0; col < n; ++col) {
            augmented[row * width + col] = source[row * n + col];
            augmented[row * width + n + col] = row == col ? 1.0L : 0.0L;
        }
    }

    for (size_t k = 0; k < n; ++k) {
        size_t pivot = k;
        for (size_t row = k + 1; row < n; ++row) {
            if (fabsl(augmented[row * width + k]) >
                fabsl(augmented[pivot * width + k])) {
                pivot = row;
            }
        }
        if (augmented[pivot * width + k] == 0.0L) {
            vec_destroy(&work);
            return false;
        }
        if (pivot != k) {
            for (size_t col = 0; col < width; ++col) {
                long double value = augmented[k * width + col];
                augmented[k * width + col] = augmented[pivot * width + col];
                augmented[pivot * width + col] = value;
            }
        }

        long double diagonal = augmented[k * width + k];
        for (size_t col = 0; col < width; ++col) {
            augmented[k * width + col] /= diagonal;
        }
        for (size_t row = 0; row < n; ++row) {
            if (row == k) continue;
            long double factor = augmented[row * width + k];
            if (factor == 0.0L) continue;
            augmented[row * width + k] = 0.0L;
            for (size_t col = k + 1; col < width; ++col) {
                augmented[row * width + col] -=
                    factor * augmented[k * width + col];
            }
        }
    }

    struct matrix temporary;
    matrix_init(&temporary);
    if (!matrix_resize(&temporary, n, n)) {
        vec_destroy(&work);
        return false;
    }
    double *inverse = temporary.elements.data;
    for (size_t row = 0; row < n; ++row) {
        for (size_t col = 0; col < n; ++col) {
            inverse[row * n + col] = (double)augmented[row * width + n + col];
        }
    }
    __matrix_replace(dst, &temporary);
    vec_destroy(&work);
    return true;
}

/* Gaussian elimination with partial pivoting. A singular matrix has det = 0. */
static inline bool matrix_det(const struct matrix *matrix, double *result)
{
    if (matrix->rows == 0 || matrix->rows != matrix->cols) return false;
    size_t n = matrix->rows;
    VEC(work, long double);
    if (!vec_resize(&work, matrix->elements.size)) return false;
    long double *a = work.data;
    const double *source = matrix->elements.data;
    for (size_t i = 0; i < work.size; ++i) a[i] = source[i];
    long double determinant = 1.0L;
    for (size_t k = 0; k < n; ++k) {
        size_t pivot = k;
        for (size_t i = k + 1; i < n; ++i) {
            if (fabsl(a[i * n + k]) > fabsl(a[pivot * n + k])) pivot = i;
        }
        if (a[pivot * n + k] == 0.0L) {
            determinant = 0.0L;
            break;
        }
        if (pivot != k) {
            for (size_t j = k; j < n; ++j) {
                long double value = a[k * n + j];
                a[k * n + j] = a[pivot * n + j];
                a[pivot * n + j] = value;
            }
            determinant = -determinant;
        }
        determinant *= a[k * n + k];
        for (size_t i = k + 1; i < n; ++i) {
            long double factor = a[i * n + k] / a[k * n + k];
            a[i * n + k] = 0.0L;
            for (size_t j = k + 1; j < n; ++j) {
                a[i * n + j] -= factor * a[k * n + j];
            }
        }
    }
    *result = (double)determinant;
    vec_destroy(&work);
    return true;
}

/* Build a unit vector v for the reflector I - 2*v*v^T. Return the new first
 * component of the reflected column, or zero for an already zero column.
 */
static inline long double __matrix_householder(long double *v, size_t count)
{
    long double norm = 0.0L;
    for (size_t i = 0; i < count; ++i) norm = hypotl(norm, v[i]);
    if (norm == 0.0L) return 0.0L;
    long double first = -copysignl(norm, v[0]);
    v[0] -= first;
    norm = 0.0L;
    for (size_t i = 0; i < count; ++i) norm = hypotl(norm, v[i]);
    for (size_t i = 0; i < count; ++i) v[i] /= norm;
    return first;
}

/* Apply the same reflector on the left and right, preserving eigenvalues. */
static inline void __matrix_reflect(long double *a, size_t n,
                                     size_t start, size_t count,
                                     const long double *v,
                                     size_t first_col, size_t rows)
{
    for (size_t j = first_col; j < n; ++j) {
        long double dot = 0.0L;
        for (size_t k = 0; k < count; ++k) dot += v[k] * a[(start + k) * n + j];
        for (size_t k = 0; k < count; ++k) a[(start + k) * n + j] -= 2.0L * v[k] * dot;
    }
    for (size_t i = 0; i < rows; ++i) {
        long double dot = 0.0L;
        for (size_t k = 0; k < count; ++k) dot += a[i * n + start + k] * v[k];
        for (size_t k = 0; k < count; ++k) a[i * n + start + k] -= 2.0L * dot * v[k];
    }
}

static inline void __matrix_hessenberg(long double *a, size_t n, long double *v)
{
    for (size_t k = 0; k + 2 < n; ++k) {
        size_t count = n - k - 1;
        for (size_t i = 0; i < count; ++i) v[i] = a[(k + 1 + i) * n + k];
        long double first = __matrix_householder(v, count);
        if (first == 0.0L) continue;
        __matrix_reflect(a, n, k + 1, count, v, k, n);
        a[(k + 1) * n + k] = first;
        for (size_t i = k + 2; i < n; ++i) a[i * n + k] = 0.0L;
    }
}

/* Solve the trailing 2-by-2 block, including complex conjugate roots. */
static inline void __matrix_eigen_pair(long double a, long double b,
                                        long double c, long double d,
                                        struct matrix_eigenvalue *first,
                                        struct matrix_eigenvalue *second)
{
    long double half_difference = (a - d) / 2.0L;
    long double product = b * c;
    long double discriminant = half_difference * half_difference + product;
    if (discriminant < 0.0L) {
        first->real = second->real = (double)(d + half_difference);
        first->imag = (double)sqrtl(-discriminant);
        second->imag = -first->imag;
    } else {
        long double root = half_difference + copysignl(sqrtl(discriminant), half_difference);
        first->real = (double)(d + root);
        second->real = (double)(root == 0.0L ? d : d - product / root);
        first->imag = second->imag = 0.0;
    }
}

/* Compute all eigenvalues of a finite real square matrix using Hessenberg
 * reduction and implicit double-shift QR. Output order is unspecified.
 * values must be initialized for struct matrix_eigenvalue elements.
 * Returns false for a non-square/empty matrix, allocation failure or failure
 * to converge. The source and previous output remain unchanged on failure.
 */
static inline bool matrix_eigenvalues(const struct matrix *matrix, struct vec *values)
{
    if (matrix->rows == 0 || matrix->rows != matrix->cols) return false;
    size_t n = matrix->rows;
    VEC(work, long double);
    VEC(vector, long double);
    VEC(roots, struct matrix_eigenvalue);
    bool success = false;
    if (!vec_resize(&work, matrix->elements.size) ||
        !vec_resize(&vector, n) || !vec_resize(&roots, n)) goto done;
    long double *a = work.data;
    long double *v = vector.data;
    struct matrix_eigenvalue *eigenvalues = roots.data;
    const double *source = matrix->elements.data;
    for (size_t i = 0; i < work.size; ++i) a[i] = source[i];
    __matrix_hessenberg(a, n, v);

    size_t high = n;
    size_t iterations = 0;
    while (high > 0) {
        size_t last = high - 1;
        size_t low = last;
        while (low > 0) {
            long double lower = fabsl(a[low * n + low - 1]);
            if (lower == 0.0L) break;
            long double diagonal = fabsl(a[(low - 1) * n + low - 1]) + fabsl(a[low * n + low]);
            if (lower <= LDBL_EPSILON * diagonal) {
                /* Keep a tiny subdiagonal when its coupling to the upper
                 * entry still changes the eigenvalues appreciably.
                 */
                long double upper = fabsl(a[(low - 1) * n + low]);
                long double ab = fmaxl(lower, upper);
                long double ba = fminl(lower, upper);
                long double difference = fabsl(a[(low - 1) * n + low - 1] - a[low * n + low]);
                long double aa = fmaxl(fabsl(a[low * n + low]), difference);
                long double bb = fminl(fabsl(a[low * n + low]), difference);
                long double scale = aa + ab;
                if (ba * (ab / scale) <= LDBL_EPSILON * bb * (aa / scale)) {
                    a[low * n + low - 1] = 0.0L;
                    break;
                }
            }
            --low;
        }
        if (low == last) {
            eigenvalues[last] = (struct matrix_eigenvalue){ .real = (double)a[last * n + last] };
            --high;
            iterations = 0;
            continue;
        }
        if (low + 1 == last) {
            __matrix_eigen_pair(a[low * n + low], a[low * n + last],
                                a[last * n + low], a[last * n + last],
                                &eigenvalues[low], &eigenvalues[last]);
            high -= 2;
            iterations = 0;
            continue;
        }
        if (++iterations > 1000 * (high - low)) goto done;

        long double x = a[last * n + last];
        long double y = a[(last - 1) * n + last - 1];
        long double product = a[last * n + last - 1] * a[(last - 1) * n + last];
        if (iterations % 20 == 0) {
            /* Exceptional shifts break stalled QR cycles. */
            long double scale = fabsl(a[last * n + last - 1]) +
                                fabsl(a[(last - 1) * n + last - 2]);
            x += 0.75L * scale;
            y = x;
            product = -0.4375L * scale * scale;
        }
        for (size_t k = low; k < last; ++k) {
            size_t count = k + 2 <= last ? 3 : 2;
            if (k == low) {
                long double diagonal = a[k * n + k];
                long double lower = a[(k + 1) * n + k];
                v[0] = (diagonal - x) * (diagonal - y) - product + a[k * n + k + 1] * lower;
                v[1] = lower * (diagonal + a[(k + 1) * n + k + 1] - x - y);
                v[2] = lower * a[(k + 2) * n + k + 1];
            } else {
                for (size_t i = 0; i < count; ++i) v[i] = a[(k + i) * n + k - 1];
            }
            long double first = __matrix_householder(v, count);
            if (first == 0.0L) continue;
            __matrix_reflect(a, n, k, count, v, k == low ? k : k - 1, high);
            if (k != low) {
                a[k * n + k - 1] = first;
                for (size_t i = 1; i < count; ++i) a[(k + i) * n + k - 1] = 0.0L;
            }
        }
    }
    vec_swap(values, &roots);
    success = true;

done:
    vec_destroy(&work);
    vec_destroy(&vector);
    vec_destroy(&roots);
    return success;
}

#endif /* MATRIX_H */
