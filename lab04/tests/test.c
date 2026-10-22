/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#include "container_of.h"
#include "expression.h"
#include "list.h"
#include "matrix.h"
#include "polynomial.h"
#include "vec.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(condition)                                                   \
    do {                                                                    \
        if (!(condition)) {                                                 \
            fprintf(stderr, "  failed: %s:%d: %s\n",                      \
                    __FILE__, __LINE__, #condition);                       \
            return false;                                                   \
        }                                                                   \
    } while (0)

static int compare_int(const void *lhs, const void *rhs)
{
    int left = *(const int *)lhs;
    int right = *(const int *)rhs;
    return (left > right) - (left < right);
}

static bool test_vec_lifecycle_and_access(void)
{
    struct vec vector;
    vec_init(&vector, int);
    CHECK(vec_empty(&vector));
    CHECK(vec_elem_size(&vector) == sizeof(int));

    int values[] = { 10, 20, 30 };
    for (size_t i = 0; i < sizeof(values) / sizeof(values[0]); ++i) {
        CHECK(vec_push_back(&vector, &values[i]));
    }
    CHECK(vec_size(&vector) == 3);
    CHECK(vec_capacity(&vector) >= 3);
    CHECK(*(int *)vec_front(&vector) == 10);
    CHECK(*(int *)vec_back(&vector) == 30);
    CHECK(*(int *)vec_at_checked(&vector, 1) == 20);
    CHECK(vec_at_checked(&vector, 3) == NULL);

    vec_destroy(&vector);
    CHECK(vector.data == NULL);
    CHECK(vector.size == 0);
    CHECK(vector.capacity == 0);
    CHECK(vector.elem_size == 0);
    return true;
}

static bool test_vec_insert_erase_and_assign(void)
{
    struct vec vector;
    vec_init(&vector, int);
    int initial[] = { 1, 3 };
    CHECK(vec_append(&vector, initial, 2));

    int two = 2;
    CHECK(vec_insert(&vector, 1, &two));
    int tail[] = { 4, 5 };
    CHECK(vec_insert_range(&vector, vector.size, tail, 2));
    CHECK(vector.size == 5);
    CHECK(*(int *)vec_at(&vector, 0) == 1);
    CHECK(*(int *)vec_at(&vector, 4) == 5);

    vec_erase(&vector, 1);
    CHECK(*(int *)vec_at(&vector, 1) == 3);
    vec_erase_n(&vector, 1, 2);
    CHECK(vector.size == 2);
    CHECK(*(int *)vec_at(&vector, 0) == 1);
    CHECK(*(int *)vec_at(&vector, 1) == 5);

    int replacement[] = { 7, 8, 9 };
    CHECK(vec_assign_range(&vector, replacement, 3));
    CHECK(vector.size == 3);
    int repeated = 4;
    CHECK(vec_assign_n(&vector, 2, &repeated));
    CHECK(vector.size == 2);
    CHECK(*(int *)vec_at(&vector, 0) == 4);
    CHECK(*(int *)vec_at(&vector, 1) == 4);

    vec_destroy(&vector);
    return true;
}

static bool test_vec_resize_capacity_and_swap(void)
{
    struct vec vector;
    vec_init(&vector, int);
    CHECK(vec_reserve(&vector, 16));
    CHECK(vector.capacity >= 16);
    CHECK(vec_resize_zero(&vector, 4));
    for (size_t i = 0; i < vector.size; ++i) {
        CHECK(*(int *)vec_at(&vector, i) == 0);
    }

    int value = 11;
    CHECK(vec_assign_n(&vector, 1, &value));
    CHECK(vec_resize(&vector, 4));
    CHECK(*(int *)vec_at(&vector, 0) == 11);
    CHECK(vec_resize(&vector, 1));
    CHECK(vec_shrink_to_fit(&vector));
    CHECK(vector.capacity == vector.size);

    struct vec other;
    vec_init(&other, int);
    int other_value = 22;
    CHECK(vec_push_back(&other, &other_value));
    vec_swap(&vector, &other);
    CHECK(*(int *)vec_front(&vector) == 22);
    CHECK(*(int *)vec_front(&other) == 11);
    volatile size_t impossible_capacity = SIZE_MAX;
    CHECK(!vec_reserve(&vector, impossible_capacity));

    vec_destroy(&vector);
    vec_destroy(&other);
    return true;
}

static bool test_vec_iterators_and_comparison(void)
{
    struct vec vector;
    vec_init(&vector, int);
    int values[] = { 1, 2, 3 };
    CHECK(vec_append(&vector, values, 3));

    int *begin = vec_begin_as(&vector, int);
    int *end = vec_end_as(&vector, int);
    CHECK(vec_iter_distance(begin, end) == 3);
    CHECK(vec_iter_distance((int *)NULL, (int *)NULL) == 0);
    CHECK(*begin == 1);
    CHECK(*(end - 1) == 3);

    struct vec_reverse_iterator reverse = vec_rbegin(&vector);
    struct vec_reverse_iterator reverse_end = vec_rend(&vector);
    CHECK(vec_reverse_distance(reverse, reverse_end) == 3);
    CHECK(*(int *)vec_reverse_deref(reverse) == 3);
    vec_reverse_next(&reverse);
    CHECK(*(int *)vec_reverse_deref(reverse) == 2);
    vec_reverse_next(&reverse);
    CHECK(*(int *)vec_reverse_deref(reverse) == 1);

    struct vec same;
    vec_init(&same, int);
    CHECK(vec_append(&same, values, 3));
    CHECK(vec_equal(&vector, &same, compare_int));
    CHECK(vec_compare(&vector, &same, compare_int) == 0);
    vec_pop_back(&same);
    CHECK(vec_compare(&same, &vector, compare_int) < 0);

    vec_destroy(&vector);
    vec_destroy(&same);
    return true;
}

struct list_test_item {
    int value;
    struct list_head link;
};

static bool test_list_and_container_of(void)
{
    LIST_HEAD(head);
    struct list_test_item first = { .value = 1 };
    struct list_test_item second = { .value = 2 };
    struct list_test_item third = { .value = 3 };
    INIT_LIST_HEAD(&first.link);
    INIT_LIST_HEAD(&second.link);
    INIT_LIST_HEAD(&third.link);

    list_add(&first.link, &head);
    list_add_tail(&second.link, &head);
    list_add_tail(&third.link, &head);
    CHECK(!list_empty(&head));
    CHECK(list_is_first(&first.link, &head));
    CHECK(list_is_last(&third.link, &head));
    CHECK(list_entry(head.next, struct list_test_item, link)->value == 1);
    CHECK(list_entry(head.prev, struct list_test_item, link)->value == 3);

    struct list_test_item *cursor;
    int expected = 1;
    list_for_each_entry(cursor, &head, link) {
        CHECK(cursor->value == expected);
        ++expected;
    }
    list_move_tail(&first.link, &head);
    CHECK(list_entry(head.prev, struct list_test_item, link)->value == 1);
    list_del(&second.link);
    CHECK(list_entry(head.next, struct list_test_item, link)->value == 3);
    CHECK(list_entry_is_head(list_entry(head.next, struct list_test_item, link),
                             &head, link) == false);
    return true;
}

static bool test_poly_vec_normalization(void)
{
    struct poly_vec poly;
    poly_vec_init(&poly);
    CHECK(poly_vec_add_term(&poly, 3.0, 2));
    CHECK(poly_vec_add_term(&poly, 1.0, 5));
    CHECK(poly_vec_add_term(&poly, 2.0, 2));
    CHECK(poly_vec_size(&poly) == 2);
    CHECK(poly_vec_at_const(&poly, 0)->exp == 5);
    CHECK(poly_vec_at_const(&poly, 0)->coef == 1.0);
    CHECK(poly_vec_at_const(&poly, 1)->exp == 2);
    CHECK(poly_vec_at_const(&poly, 1)->coef == 5.0);
    CHECK(poly_vec_set_term(&poly, 0.0, 5));
    CHECK(!poly_vec_get_term(&poly, 5, &(double){ 0.0 }));
    CHECK(poly_vec_remove_term(&poly, 2));
    CHECK(poly_vec_empty(&poly));
    poly_vec_destroy(&poly);
    return true;
}

static bool test_poly_vec_add_sub_and_alias(void)
{
    struct poly_vec a;
    struct poly_vec b;
    struct poly_vec result;
    poly_vec_init(&a);
    poly_vec_init(&b);
    poly_vec_init(&result);
    CHECK(poly_vec_add_term(&a, 3.0, 2));
    CHECK(poly_vec_add_term(&a, 2.0, 1));
    CHECK(poly_vec_add_term(&a, 1.0, 0));
    CHECK(poly_vec_add_term(&b, 1.0, 2));
    CHECK(poly_vec_add_term(&b, -2.0, 1));
    CHECK(poly_vec_add_term(&b, 4.0, 0));

    CHECK(poly_vec_add(&result, &a, &b));
    CHECK(poly_vec_equal(&result, &b, 0.0) == false);
    CHECK(poly_vec_get_term(&result, 2, &(double){ 0.0 }));
    CHECK(result.terms.size == 2);
    CHECK(poly_vec_at_const(&result, 0)->coef == 4.0);
    CHECK(poly_vec_at_const(&result, 1)->exp == 0);
    CHECK(poly_vec_at_const(&result, 1)->coef == 5.0);

    CHECK(poly_vec_sub(&result, &a, &b));
    CHECK(poly_vec_at_const(&result, 0)->coef == 2.0);
    CHECK(poly_vec_at_const(&result, 1)->coef == 4.0);
    CHECK(poly_vec_at_const(&result, 2)->coef == -3.0);

    CHECK(poly_vec_add(&a, &a, &b));
    CHECK(a.terms.size == 2);
    CHECK(poly_vec_at_const(&a, 0)->coef == 4.0);
    CHECK(poly_vec_at_const(&a, 1)->coef == 5.0);

    poly_vec_destroy(&a);
    poly_vec_destroy(&b);
    poly_vec_destroy(&result);
    return true;
}

static bool test_poly_vec_mul_derivative_and_eval(void)
{
    struct poly_vec x_plus_one;
    struct poly_vec x_minus_one;
    struct poly_vec result;
    struct poly_vec derivative;
    poly_vec_init(&x_plus_one);
    poly_vec_init(&x_minus_one);
    poly_vec_init(&result);
    poly_vec_init(&derivative);
    CHECK(poly_vec_add_term(&x_plus_one, 1.0, 1));
    CHECK(poly_vec_add_term(&x_plus_one, 1.0, 0));
    CHECK(poly_vec_add_term(&x_minus_one, 1.0, 1));
    CHECK(poly_vec_add_term(&x_minus_one, -1.0, 0));
    CHECK(poly_vec_mul(&result, &x_plus_one, &x_minus_one));
    CHECK(result.terms.size == 2);
    CHECK(poly_vec_at_const(&result, 0)->exp == 2);
    CHECK(poly_vec_at_const(&result, 0)->coef == 1.0);
    CHECK(poly_vec_at_const(&result, 1)->exp == 0);
    CHECK(poly_vec_at_const(&result, 1)->coef == -1.0);
    CHECK(poly_vec_derivative(&derivative, &result, 1));
    CHECK(derivative.terms.size == 1);
    CHECK(poly_vec_at_const(&derivative, 0)->coef == 2.0);
    CHECK(poly_vec_eval(&result, 3.0) == 8.0);

    poly_vec_destroy(&x_plus_one);
    poly_vec_destroy(&x_minus_one);
    poly_vec_destroy(&result);
    poly_vec_destroy(&derivative);
    return true;
}

static bool test_poly_list_normalization_and_conversion(void)
{
    struct poly_list list;
    struct poly_vec vector;
    struct poly_list copy;
    poly_list_init(&list);
    poly_vec_init(&vector);
    poly_list_init(&copy);
    CHECK(poly_list_add_term(&list, 2.0, 1));
    CHECK(poly_list_add_term(&list, 5.0, 4));
    CHECK(poly_list_add_term(&list, 3.0, 1));
    CHECK(poly_list_size(&list) == 2);
    CHECK(poly_list_get_term(&list, 1, &(double){ 0.0 }));
    CHECK(list_entry(list.head.next, struct poly_node, link)->term.exp == 4);
    CHECK(poly_vec_from_list(&vector, &list));
    CHECK(poly_vec_size(&vector) == 2);
    CHECK(poly_vec_at_const(&vector, 0)->exp == 4);
    CHECK(poly_vec_at_const(&vector, 1)->coef == 5.0);
    CHECK(poly_list_from_vec(&copy, &vector));
    CHECK(poly_list_equal(&list, &copy, 0.0));
    CHECK(poly_list_remove_term(&list, 4));
    CHECK(poly_list_size(&list) == 1);

    poly_list_destroy(&list);
    poly_vec_destroy(&vector);
    poly_list_destroy(&copy);
    return true;
}

static bool test_poly_list_arithmetic_and_alias(void)
{
    struct poly_list a;
    struct poly_list b;
    struct poly_list result;
    struct poly_list derivative;
    poly_list_init(&a);
    poly_list_init(&b);
    poly_list_init(&result);
    poly_list_init(&derivative);
    CHECK(poly_list_add_term(&a, 3.0, 2));
    CHECK(poly_list_add_term(&a, 1.0, 0));
    CHECK(poly_list_add_term(&b, 1.0, 2));
    CHECK(poly_list_add_term(&b, -1.0, 0));
    CHECK(poly_list_add(&result, &a, &b));
    double coefficient;
    CHECK(poly_list_get_term(&result, 2, &coefficient));
    CHECK(coefficient == 4.0);
    CHECK(!poly_list_get_term(&result, 0, &coefficient));
    CHECK(poly_list_sub(&result, &a, &b));
    CHECK(poly_list_get_term(&result, 0, &coefficient));
    CHECK(coefficient == 2.0);
    CHECK(poly_list_derivative(&derivative, &a, 1));
    CHECK(poly_list_get_term(&derivative, 1, &coefficient));
    CHECK(coefficient == 6.0);
    CHECK(poly_list_eval(&a, 2.0) == 13.0);
    CHECK(poly_list_add(&a, &a, &b));
    CHECK(poly_list_get_term(&a, 2, &coefficient));
    CHECK(coefficient == 4.0);

    poly_list_destroy(&a);
    poly_list_destroy(&b);
    poly_list_destroy(&result);
    poly_list_destroy(&derivative);
    return true;
}

static bool test_polynomial_integral(void)
{
    struct poly_vec vector, vector_result;
    struct poly_list list, list_result;
    poly_vec_init(&vector);
    poly_vec_init(&vector_result);
    poly_list_init(&list);
    poly_list_init(&list_result);

    CHECK(poly_vec_add_term(&vector, 6.0, 3));
    CHECK(poly_vec_add_term(&vector, -4.0, 1));
    CHECK(poly_vec_add_term(&vector, 2.0, 0));
    CHECK(poly_vec_integral(&vector_result, &vector));
    CHECK(vector_result.terms.size == 3);
    CHECK(poly_vec_get_term(&vector_result, 4, &(double){ 0.0 }));
    CHECK(poly_vec_at_const(&vector_result, 0)->coef == 1.5);
    CHECK(poly_vec_at_const(&vector_result, 1)->coef == -2.0);
    CHECK(poly_vec_at_const(&vector_result, 2)->coef == 2.0);
    CHECK(poly_vec_integral(&vector_result, &vector_result));
    CHECK(poly_vec_at_const(&vector_result, 0)->exp == 5);
    CHECK(poly_vec_at_const(&vector_result, 0)->coef == 0.3);
    CHECK(poly_vec_at_const(&vector_result, 1)->exp == 3);
    CHECK(poly_vec_at_const(&vector_result, 1)->coef == -2.0 / 3.0);
    CHECK(poly_vec_at_const(&vector_result, 2)->exp == 2);
    CHECK(poly_vec_at_const(&vector_result, 2)->coef == 1.0);

    CHECK(poly_list_add_term(&list, 6.0, 3));
    CHECK(poly_list_add_term(&list, -4.0, 1));
    CHECK(poly_list_add_term(&list, 2.0, 0));
    CHECK(poly_list_integral(&list_result, &list));
    double coefficient;
    CHECK(poly_list_get_term(&list_result, 4, &coefficient));
    CHECK(coefficient == 1.5);
    CHECK(poly_list_get_term(&list_result, 2, &coefficient));
    CHECK(coefficient == -2.0);
    CHECK(poly_list_get_term(&list_result, 1, &coefficient));
    CHECK(coefficient == 2.0);
    CHECK(poly_list_integral(&list_result, &list_result));
    CHECK(poly_list_get_term(&list_result, 5, &coefficient));
    CHECK(coefficient == 0.3);
    CHECK(poly_list_get_term(&list_result, 3, &coefficient));
    CHECK(coefficient == -2.0 / 3.0);
    CHECK(poly_list_get_term(&list_result, 2, &coefficient));
    CHECK(coefficient == 1.0);

    CHECK(poly_vec_set_term(&vector_result, 9.0, 0));
    CHECK(poly_vec_set_term(&vector, 1.0, UINT_MAX));
    CHECK(!poly_vec_integral(&vector_result, &vector));
    CHECK(poly_vec_get_term(&vector_result, 0, &coefficient));
    CHECK(coefficient == 9.0);
    CHECK(poly_list_set_term(&list_result, 9.0, 0));
    CHECK(poly_list_set_term(&list, 1.0, UINT_MAX));
    CHECK(!poly_list_integral(&list_result, &list));
    CHECK(poly_list_get_term(&list_result, 0, &coefficient));
    CHECK(coefficient == 9.0);

    poly_vec_destroy(&vector);
    poly_vec_destroy(&vector_result);
    poly_list_destroy(&list);
    poly_list_destroy(&list_result);
    return true;
}

static bool test_polynomial_integral_edges(void)
{
    struct poly_vec vector, vector_result;
    struct poly_list list, list_result;
    poly_vec_init(&vector);
    poly_vec_init(&vector_result);
    poly_list_init(&list);
    poly_list_init(&list_result);

    CHECK(poly_vec_add_term(&vector_result, 9.0, 0));
    CHECK(poly_vec_integral(&vector_result, &vector));
    CHECK(poly_vec_empty(&vector_result));
    CHECK(poly_list_add_term(&list_result, 9.0, 0));
    CHECK(poly_list_integral(&list_result, &list));
    CHECK(poly_list_empty(&list_result));

    CHECK(poly_vec_add_term(&vector, 7.0, 0));
    CHECK(poly_vec_integral(&vector_result, &vector));
    CHECK(vector_result.terms.size == 1);
    CHECK(poly_vec_at_const(&vector_result, 0)->coef == 7.0);
    CHECK(poly_vec_at_const(&vector_result, 0)->exp == 1);
    CHECK(poly_list_add_term(&list, 7.0, 0));
    CHECK(poly_list_integral(&list_result, &list));
    double coefficient;
    CHECK(poly_list_get_term(&list_result, 1, &coefficient));
    CHECK(coefficient == 7.0);

    CHECK(poly_vec_set_term(&vector, 1.0, 2));
    CHECK(poly_vec_integral(&vector_result, &vector));
    CHECK(fabs(poly_vec_at_const(&vector_result, 0)->coef - 1.0 / 3.0) < 1e-15);
    CHECK(poly_vec_at_const(&vector_result, 0)->exp == 3);
    CHECK(poly_vec_get_term(&vector, 2, &coefficient));
    CHECK(coefficient == 1.0);
    CHECK(poly_list_set_term(&list, 1.0, 2));
    CHECK(poly_list_integral(&list_result, &list));
    CHECK(poly_list_get_term(&list_result, 3, &coefficient));
    CHECK(fabs(coefficient - 1.0 / 3.0) < 1e-15);
    CHECK(poly_list_get_term(&list, 2, &coefficient));
    CHECK(coefficient == 1.0);

    CHECK(poly_vec_set_term(&vector_result, 8.0, 2));
    CHECK(poly_vec_set_term(&vector_result, 9.0, 0));
    CHECK(poly_vec_set_term(&vector, 1.0, UINT_MAX));
    CHECK(!poly_vec_integral(&vector_result, &vector));
    CHECK(poly_vec_get_term(&vector_result, 2, &coefficient));
    CHECK(coefficient == 8.0);
    CHECK(poly_vec_get_term(&vector_result, 0, &coefficient));
    CHECK(coefficient == 9.0);

    CHECK(poly_list_set_term(&list_result, 8.0, 2));
    CHECK(poly_list_set_term(&list_result, 9.0, 0));
    CHECK(poly_list_set_term(&list, 1.0, UINT_MAX));
    CHECK(!poly_list_integral(&list_result, &list));
    CHECK(poly_list_get_term(&list_result, 2, &coefficient));
    CHECK(coefficient == 8.0);
    CHECK(poly_list_get_term(&list_result, 0, &coefficient));
    CHECK(coefficient == 9.0);

    poly_vec_destroy(&vector);
    poly_vec_destroy(&vector_result);
    poly_list_destroy(&list);
    poly_list_destroy(&list_result);
    return true;
}

static bool test_vec_mutation_edges(void)
{
    struct vec vector;
    vec_init(&vector, int);
    int values[] = { 1, 2, 3 };
    CHECK(vec_append(&vector, values, 3));

    int *slot = vec_emplace_as(&vector, int, 1);
    CHECK(slot != NULL);
    *slot = 9;
    slot = vec_emplace_back_as(&vector, int);
    CHECK(slot != NULL);
    *slot = 4;
    int popped;
    CHECK(vec_pop_back_copy(&vector, &popped));
    CHECK(popped == 4);
    CHECK(vec_erase_range(&vector, 1, 3) == (int *)vector.data + 1);
    CHECK(vector.size == 2);
    CHECK(*(int *)vec_at(&vector, 0) == 1);
    CHECK(*(int *)vec_at(&vector, 1) == 3);
    CHECK(vec_erase_range(&vector, 2, 1) == NULL);
    vec_swap_erase(&vector, 0);
    CHECK(vector.size == 1 && *(int *)vec_front(&vector) == 3);

    vec_clear(&vector);
    CHECK(vec_empty(&vector));
    CHECK(vec_append(&vector, values, 3));
    CHECK(vec_push_back(&vector, vec_at(&vector, 1)));
    CHECK(vector.size == 4 && *(int *)vec_back(&vector) == 2);
    CHECK(vec_assign_n(&vector, 3, vec_at(&vector, 1)));
    CHECK(vector.size == 3);
    for (size_t i = 0; i < vector.size; ++i) {
        CHECK(*(int *)vec_at(&vector, i) == 2);
    }
    vec_destroy(&vector);
    return true;
}

static bool test_list_replace_and_move(void)
{
    LIST_HEAD(head);
    struct list_test_item first = { .value = 1 };
    struct list_test_item second = { .value = 2 };
    struct list_test_item third = { .value = 3 };
    struct list_test_item replacement = { .value = 4 };
    INIT_LIST_HEAD(&first.link);
    INIT_LIST_HEAD(&second.link);
    INIT_LIST_HEAD(&third.link);
    INIT_LIST_HEAD(&replacement.link);
    list_add_tail(&first.link, &head);
    list_add_tail(&second.link, &head);
    list_add_tail(&third.link, &head);

    list_replace_init(&second.link, &replacement.link);
    CHECK(list_entry(head.next, struct list_test_item, link)->value == 1);
    CHECK(list_entry(head.next->next, struct list_test_item, link)->value == 4);
    CHECK(second.link.next == &second.link && second.link.prev == &second.link);
    list_move(&third.link, &head);
    CHECK(list_entry(head.next, struct list_test_item, link)->value == 3);
    list_move_tail(&first.link, &head);
    CHECK(list_entry(head.prev, struct list_test_item, link)->value == 1);
    CHECK(list_is_first(&third.link, &head));
    CHECK(list_is_last(&first.link, &head));
    list_del(&replacement.link);
    CHECK(head.next->next == &first.link);
    list_del(&third.link);
    list_del(&first.link);
    CHECK(list_empty(&head));
    return true;
}

static bool test_polynomial_copy_and_derivative_edges(void)
{
    struct poly_vec vector, vector_copy, derivative;
    struct poly_list list, list_copy, list_derivative;
    poly_vec_init(&vector);
    poly_vec_init(&vector_copy);
    poly_vec_init(&derivative);
    poly_list_init(&list);
    poly_list_init(&list_copy);
    poly_list_init(&list_derivative);
    CHECK(poly_vec_add_term(&vector, 3.0, 2));
    CHECK(poly_vec_add_term(&vector, 2.0, 1));
    CHECK(poly_vec_add_term(&vector, 1.0, 0));
    CHECK(poly_vec_copy(&vector_copy, &vector));
    CHECK(poly_vec_equal(&vector, &vector_copy, 0.0));
    CHECK(poly_vec_derivative(&derivative, &vector, 0));
    CHECK(poly_vec_equal(&derivative, &vector, 0.0));
    CHECK(poly_vec_derivative(&derivative, &vector, 1));
    CHECK(derivative.terms.size == 2);
    CHECK(poly_vec_at_const(&derivative, 0)->exp == 1);
    CHECK(poly_vec_at_const(&derivative, 0)->coef == 6.0);
    CHECK(poly_vec_at_const(&derivative, 1)->exp == 0);
    CHECK(poly_vec_at_const(&derivative, 1)->coef == 2.0);
    CHECK(poly_vec_derivative(&derivative, &vector, 3));
    CHECK(poly_vec_empty(&derivative));

    CHECK(poly_list_from_vec(&list, &vector));
    CHECK(poly_list_copy(&list_copy, &list));
    CHECK(poly_list_equal(&list, &list_copy, 0.0));
    CHECK(poly_list_derivative(&list_derivative, &list, 0));
    CHECK(poly_list_equal(&list_derivative, &list, 0.0));
    CHECK(poly_list_derivative(&list_derivative, &list, 1));
    double coefficient;
    CHECK(poly_list_get_term(&list_derivative, 1, &coefficient));
    CHECK(coefficient == 6.0);
    CHECK(poly_list_get_term(&list_derivative, 0, &coefficient));
    CHECK(coefficient == 2.0);
    CHECK(poly_list_derivative(&list_derivative, &list, 3));
    CHECK(poly_list_empty(&list_derivative));

    poly_vec_destroy(&vector);
    poly_vec_destroy(&vector_copy);
    poly_vec_destroy(&derivative);
    poly_list_destroy(&list);
    poly_list_destroy(&list_copy);
    poly_list_destroy(&list_derivative);
    return true;
}

static bool test_expression_nested_precedence_and_variables(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context,
        "-(A + 2.5) * (B - (-C + 4)) / 2", &error));
    CHECK(context.variables.size == 3);
    CHECK(expression_set_variable(&context, "A", 3.0));
    CHECK(expression_set_variable(&context, "B", 10.0));
    CHECK(expression_set_variable(&context, "C", 2.0));
    CHECK(expression_eval(&context,
        "-(A + 2.5) * (B - (-C + 4)) / 2", &result, &error));
    CHECK(fabs(result + 22.0) < 1e-12);
    expression_destroy(&context);
    return true;
}

static bool test_expression_variable_dedup_and_update(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context,
        "A1 + A1 * B2 - (B2 / 2) + C", &error));
    CHECK(context.variables.size == 3);
    CHECK(expression_set_variable(&context, "A1", 4.0));
    CHECK(expression_set_variable(&context, "B2", 6.0));
    CHECK(expression_set_variable(&context, "C", 1.0));
    CHECK(expression_eval(&context,
        "A1 + A1 * B2 - (B2 / 2) + C", &result, &error));
    CHECK(fabs(result - 26.0) < 1e-12);
    CHECK(expression_set_variable(&context, "B2", 10.0));
    CHECK(expression_eval(&context,
        "A1 + A1 * B2 - (B2 / 2) + C", &result, &error));
    CHECK(result == 40.0);
    expression_destroy(&context);
    return true;
}

static bool test_expression_unary_and_decimal_values(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context,
        "--5 + -(+2.5) * (3 - -1) + .5", &error));
    CHECK(expression_eval(&context,
        "--5 + -(+2.5) * (3 - -1) + .5", &result, &error));
    CHECK(fabs(result + 4.5) < 1e-12);
    expression_destroy(&context);
    return true;
}

static bool test_expression_deep_parentheses_and_associativity(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context,
        "1 - 2 - 3 + 4 * (5 + (6 - (7 - 8)))", &error));
    CHECK(expression_eval(&context,
        "1 - 2 - 3 + 4 * (5 + (6 - (7 - 8)))", &result, &error));
    CHECK(fabs(result - 44.0) < 1e-12);
    expression_destroy(&context);
    return true;
}

static bool test_expression_error_paths(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context, "X + 1", &error));
    CHECK(!expression_eval(&context, "X + 1", &result, &error));
    CHECK(error.code == EXPRESSION_UNSET_VARIABLE);
    CHECK(expression_set_variable(&context, "X", 2.0));
    CHECK(!expression_eval(&context, "X / (3 - 3)", &result, &error));
    CHECK(error.code == EXPRESSION_DIVISION_BY_ZERO);
    CHECK(!expression_eval(&context, "X + (2 * 3", &result, &error));
    CHECK(error.code == EXPRESSION_PAREN_ERROR);
    CHECK(!expression_collect_variables(&context, "X @ 2", &error));
    CHECK(error.code == EXPRESSION_SYNTAX_ERROR);
    expression_destroy(&context);
    return true;
}

static bool test_expression_power_precedence(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    const struct {
        const char *text;
        double expected;
    } cases[] = {
        { "2^3^2", 512.0 },
        { "-2^2", -4.0 },
        { "(-2)^2", 4.0 },
        { "2^-2", 0.25 },
        { "2^-2^2", 0.0625 },
        { "--2^2", 4.0 },
        { "2^--2", 4.0 },
        { "-2^-2", -0.25 },
        { "2^-2*3", 0.75 },
        { "1+2*3^2", 19.0 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_eval(&context, cases[i].text, &result, &error));
        CHECK(fabs(result - cases[i].expected) < 1e-12);
    }
    expression_destroy(&context);
    return true;
}

static bool test_expression_scientific_literals_and_variables(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    const struct {
        const char *text;
        double expected;
    } cases[] = {
        { "1.14e2", 114.0 },
        { "-1.14e2", -114.0 },
        { "2E+1+.5", 20.5 },
        { "1e-3+5e-1", 0.501 },
        { "1.+.5", 1.5 },
        { "+2.5e-2", 0.025 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_collect_variables(&context, cases[i].text, &error));
        CHECK(context.variables.size == 0);
        CHECK(expression_eval(&context, cases[i].text, &result, &error));
        CHECK(fabs(result - cases[i].expected) < 1e-12);
    }
    CHECK(expression_collect_variables(&context, "1.14e2+X2*X2-_value", &error));
    CHECK(context.variables.size == 2);
    CHECK(expression_set_variable(&context, "X2", 3.0));
    CHECK(expression_set_variable(&context, "_value", 2.0));
    CHECK(expression_eval(&context, "1.14e2+X2*X2-_value", &result, &error));
    CHECK(result == 121.0);
    expression_destroy(&context);
    return true;
}

static bool test_expression_comparisons(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    const struct {
        const char *text;
        double expected;
    } cases[] = {
        { "1<2", 1.0 }, { "2<1", 0.0 },
        { "1<=1", 1.0 }, { "2<=1", 0.0 },
        { "2>1", 1.0 }, { "1>2", 0.0 },
        { "2>=2", 1.0 }, { "1>=2", 0.0 },
        { "2==2", 1.0 }, { "1==2", 0.0 },
        { "1!=2", 1.0 }, { "2!=2", 0.0 },
        { "1+2*3>=7", 1.0 },
        { "2^3>2*3+1", 1.0 },
        { "1<2==1", 1.0 },
        { "1==2<3", 1.0 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_eval(&context, cases[i].text, &result, &error));
        CHECK(result == cases[i].expected);
    }
    expression_destroy(&context);
    return true;
}

static bool test_expression_conditional_nesting(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    const struct {
        const char *text;
        double expected;
    } cases[] = {
        { "0?8:9", 9.0 },
        { "-2?8:9", 8.0 },
        { "3<4?10:20", 10.0 },
        { "1?0:1?2:3", 0.0 },
        { "1?0?2:3:4", 3.0 },
        { "0?1?2:3:4", 4.0 },
        { "0?2:1?3:4", 3.0 },
        { "(1?2:3)?4:5", 4.0 },
        { "2*(1?3:4)+5", 11.0 },
        { "(0?1:1?2:3)*4", 8.0 },
        { "2^(1?2:3)", 4.0 },
        { "(-1?2:3)^2", 4.0 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_eval(&context, cases[i].text, &result, &error));
        CHECK(result == cases[i].expected);
    }
    expression_destroy(&context);
    return true;
}

static bool test_expression_conditional_short_circuit(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_collect_variables(&context, "X+1", &error));
    const char *cases[] = {
        "0?1/0:7",
        "1?7:1/0",
        "0?X:7",
        "1?7:X",
        "0?missing(1):7",
        "1?7:missing(1)",
        "1?(0?1/0:6)+1:1/0",
        "0?missing(X):(1?7:1/0)",
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_eval(&context, cases[i], &result, &error));
        CHECK(result == 7.0);
    }
    expression_destroy(&context);
    return true;
}

static bool test_expression_function_calls(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_define(&context, "DEF f(X2)=3+4*X2;", &error));
    CHECK(expression_define(&context, "DEF g(x)=3+4*f(x);", &error));
    const struct {
        const char *text;
        double expected;
    } cases[] = {
        { "f(5)", 23.0 },
        { "g(5)", 95.0 },
        { "f(2+3)", 23.0 },
        { "f(f(1))", 31.0 },
        { "f(g(1))", 127.0 },
        { "-f(1)^2", -49.0 },
        { "f(1?5:1/0)", 23.0 },
        { "f(1?2:1/0)+g(0?1/0:1)", 42.0 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(expression_eval(&context, cases[i].text, &result, &error));
        CHECK(result == cases[i].expected);
    }
    CHECK(expression_collect_variables(&context, "f(X2)+f(X2)", &error));
    CHECK(context.variables.size == 1);
    CHECK(expression_set_variable(&context, "X2", 5.0));
    CHECK(expression_eval(&context, "f(X2)+f(X2)", &result, &error));
    CHECK(result == 46.0);
    CHECK(expression_eval(&context, "X2", &result, &error));
    CHECK(result == 5.0);
    expression_destroy(&context);
    return true;
}

static bool test_expression_recursive_calls(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_define(&context, "DEF fact(n)=n<=1?1:n*fact(n-1);", &error));
    CHECK(expression_eval(&context, "fact(0)", &result, &error));
    CHECK(result == 1.0);
    CHECK(expression_eval(&context, "fact(5)", &result, &error));
    CHECK(result == 120.0);
    CHECK(expression_define(&context, "DEF fib(n)=n<=1?n:fib(n-1)+fib(n-2);", &error));
    CHECK(expression_eval(&context, "fib(10)", &result, &error));
    CHECK(result == 55.0);
    CHECK(expression_define(&context, "DEF even(n)=n==0?1:odd(n-1);", &error));
    CHECK(expression_define(&context, "DEF odd(n)=n==0?0:even(n-1);", &error));
    CHECK(expression_eval(&context, "even(8)+odd(9)", &result, &error));
    CHECK(result == 2.0);
    expression_destroy(&context);
    return true;
}

static bool test_expression_function_history(void)
{
    struct expression_context context;
    struct expression_error error;
    double result;
    expression_init(&context);
    CHECK(expression_define(&context, "DEF f(X2)=3+4*X2;", &error));
    CHECK(expression_define(&context, "DEF fact(n)=n<=1?1:n*fact(n-1);", &error));
    CHECK(expression_define(&context, "DEF f(X2)=100+X2;", &error));
    CHECK(expression_define(&context, "DEF fact(n)=100+n;", &error));
    CHECK(context.functions.size == 4);
    CHECK(expression_eval(&context, "f(5)", &result, &error));
    CHECK(result == 105.0);
    CHECK(expression_eval(&context, "fact(5)", &result, &error));
    CHECK(result == 105.0);
    CHECK(expression_call(&context, 0, 5.0, &result, &error));
    CHECK(result == 23.0);
    CHECK(expression_call(&context, 1, 5.0, &result, &error));
    CHECK(result == 120.0);
    CHECK(expression_call(&context, 2, 5.0, &result, &error));
    CHECK(result == 105.0);
    CHECK(expression_call(&context, 3, 5.0, &result, &error));
    CHECK(result == 105.0);
    expression_destroy(&context);
    return true;
}

static bool test_expression_definition_errors(void)
{
    struct expression_context context;
    struct expression_error error;
    expression_init(&context);
    CHECK(expression_define(&context, "DEF f(x)=x+1;", &error));
    const struct {
        const char *text;
        enum expression_error_code expected;
    } cases[] = {
        { "", EXPRESSION_SYNTAX_ERROR },
        { "DEF", EXPRESSION_SYNTAX_ERROR },
        { "DEF f()=1;", EXPRESSION_SYNTAX_ERROR },
        { "DEF 2f(x)=x;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(1)=1;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x,y)=x;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x) x;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x)=;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x)=x@2;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x)=1e999;", EXPRESSION_SYNTAX_ERROR },
        { "DEF f(x)=x+y;", EXPRESSION_UNKNOWN_VARIABLE },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        CHECK(!expression_define(&context, cases[i].text, &error));
        CHECK(error.code == cases[i].expected);
        CHECK(context.functions.size == 1);
    }
    expression_destroy(&context);
    return true;
}

static bool test_expression_conditional_and_call_errors(void)
{
    struct expression_context context;
    struct expression_error error;
    expression_init(&context);
    CHECK(expression_define(&context, "DEF f(x)=x+1;", &error));
    const struct {
        const char *text;
        enum expression_error_code expected;
    } cases[] = {
        { "missing(1)", EXPRESSION_UNKNOWN_FUNCTION },
        { "f(Y)", EXPRESSION_UNKNOWN_VARIABLE },
        { "f()", EXPRESSION_SYNTAX_ERROR },
        { "f(1,2)", EXPRESSION_SYNTAX_ERROR },
        { "f(1", EXPRESSION_PAREN_ERROR },
        { "1?", EXPRESSION_SYNTAX_ERROR },
        { "1?:2", EXPRESSION_SYNTAX_ERROR },
        { "0?1:", EXPRESSION_SYNTAX_ERROR },
        { "1?2:3:4", EXPRESSION_SYNTAX_ERROR },
        { "1:2", EXPRESSION_SYNTAX_ERROR },
        { "1<", EXPRESSION_SYNTAX_ERROR },
        { "1<=", EXPRESSION_SYNTAX_ERROR },
        { "1=1", EXPRESSION_SYNTAX_ERROR },
        { "1!1", EXPRESSION_SYNTAX_ERROR },
        { "1?(2:3", EXPRESSION_PAREN_ERROR },
        { "1?2:3?4", EXPRESSION_SYNTAX_ERROR },
        { "0?1?2:3", EXPRESSION_SYNTAX_ERROR },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        double result = 123.0;
        CHECK(!expression_eval(&context, cases[i].text, &result, &error));
        CHECK(error.code == cases[i].expected);
        CHECK(result == 123.0);
    }
    /* The last case is missing the colon for the outer question mark. */
    CHECK(error.position == 1);
    expression_destroy(&context);
    return true;
}

static bool test_matrix_storage(void)
{
    struct matrix a, b;
    matrix_init(&a);
    matrix_init(&b);
    CHECK(a.rows == 0 && a.cols == 0 && a.elements.size == 0);
    double data[] = { 1, 2, 3, 4, 5, 6 };
    CHECK(matrix_assign(&a, 2, 3, data));
    CHECK(a.rows == 2 && a.cols == 3 && a.elements.size == 6);
    CHECK(*matrix_at_const(&a, 1, 2) == 6);
    CHECK(matrix_assign(&a, 2, 3, a.elements.data));
    CHECK(matrix_copy(&b, &a));
    *matrix_at(&b, 1, 2) = 9;
    CHECK(*matrix_at_const(&a, 1, 2) == 6);
    CHECK(matrix_copy(&b, &b));
    CHECK(*matrix_at_const(&b, 1, 2) == 9);
    CHECK(matrix_resize(&a, 3, 3));
    CHECK(memcmp(a.elements.data, data, sizeof(data)) == 0);
    for (size_t i = 6; i < 9; ++i) CHECK(((double *)a.elements.data)[i] == 0);
    CHECK(matrix_resize(&a, 1, 2));
    CHECK(a.rows == 1 && a.cols == 2 && a.elements.size == 2);
    CHECK(*matrix_at_const(&a, 0, 1) == 2);
    matrix_destroy(&a);
    CHECK(a.rows == 0 && a.cols == 0 && a.elements.data == NULL);
    matrix_init(&a);
    CHECK(matrix_copy(&b, &a));
    CHECK(b.rows == 0 && b.cols == 0 && b.elements.size == 0);
    matrix_destroy(&a);
    matrix_destroy(&b);
    return true;
}

static bool test_matrix_add_sub_and_alias(void)
{
    struct matrix a, b, result;
    matrix_init(&a);
    matrix_init(&b);
    matrix_init(&result);
    double left[] = { 1, 2, 3, 4, 5, 6 };
    double right[] = { 6, 5, 4, 3, 2, 1 };
    double sum[] = { 7, 7, 7, 7, 7, 7 };
    double difference[] = { -5, -3, -1, 1, 3, 5 };
    CHECK(matrix_assign(&a, 2, 3, left));
    CHECK(matrix_assign(&b, 2, 3, right));
    CHECK(matrix_add(&result, &a, &b));
    CHECK(result.rows == 2 && result.cols == 3);
    CHECK(memcmp(result.elements.data, sum, sizeof(sum)) == 0);
    CHECK(matrix_sub(&result, &a, &b));
    CHECK(memcmp(result.elements.data, difference, sizeof(difference)) == 0);
    CHECK(memcmp(a.elements.data, left, sizeof(left)) == 0);
    CHECK(memcmp(b.elements.data, right, sizeof(right)) == 0);
    CHECK(matrix_add(&a, &a, &b));
    CHECK(memcmp(a.elements.data, sum, sizeof(sum)) == 0);
    CHECK(matrix_sub(&b, &a, &b));
    CHECK(memcmp(b.elements.data, left, sizeof(left)) == 0);
    CHECK(matrix_sub(&a, &a, &a));
    for (size_t i = 0; i < a.elements.size; ++i) {
        CHECK(((double *)a.elements.data)[i] == 0);
    }
    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&result);
    return true;
}

static bool test_matrix_multiply_and_alias(void)
{
    struct matrix a, b, result;
    matrix_init(&a);
    matrix_init(&b);
    matrix_init(&result);
    double left[] = { 1, 2, 3, 4, 5, 6 };
    double right[] = { 7, 8, 9, 10, 11, 12 };
    double product[] = { 58, 64, 139, 154 };
    CHECK(matrix_assign(&a, 2, 3, left));
    CHECK(matrix_assign(&b, 3, 2, right));
    CHECK(matrix_mul(&result, &a, &b));
    CHECK(result.rows == 2 && result.cols == 2);
    CHECK(memcmp(result.elements.data, product, sizeof(product)) == 0);
    CHECK(memcmp(a.elements.data, left, sizeof(left)) == 0);
    CHECK(memcmp(b.elements.data, right, sizeof(right)) == 0);
    CHECK(matrix_mul(&a, &a, &b));
    CHECK(a.rows == 2 && a.cols == 2);
    CHECK(memcmp(a.elements.data, product, sizeof(product)) == 0);
    CHECK(matrix_assign(&a, 2, 3, left));
    CHECK(matrix_mul(&b, &a, &b));
    CHECK(b.rows == 2 && b.cols == 2);
    CHECK(memcmp(b.elements.data, product, sizeof(product)) == 0);
    double square[] = { 1, 2, 3, 4 };
    double squared[] = { 7, 10, 15, 22 };
    CHECK(matrix_assign(&a, 2, 2, square));
    CHECK(matrix_mul(&a, &a, &a));
    CHECK(memcmp(a.elements.data, squared, sizeof(squared)) == 0);
    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&result);
    return true;
}

static bool test_matrix_transpose(void)
{
    struct matrix a, result;
    matrix_init(&a);
    matrix_init(&result);
    double data[] = { 1, 2, 3, 4, 5, 6 };
    double transposed[] = { 1, 4, 2, 5, 3, 6 };
    CHECK(matrix_assign(&a, 2, 3, data));
    CHECK(matrix_transpose(&result, &a));
    CHECK(result.rows == 3 && result.cols == 2);
    CHECK(memcmp(result.elements.data, transposed, sizeof(transposed)) == 0);
    CHECK(memcmp(a.elements.data, data, sizeof(data)) == 0);
    CHECK(matrix_transpose(&a, &a));
    CHECK(a.rows == 3 && a.cols == 2);
    CHECK(memcmp(a.elements.data, transposed, sizeof(transposed)) == 0);
    CHECK(matrix_transpose(&a, &a));
    CHECK(a.rows == 2 && a.cols == 3);
    CHECK(memcmp(a.elements.data, data, sizeof(data)) == 0);
    CHECK(matrix_assign(&a, 1, 6, data));
    CHECK(matrix_transpose(&a, &a));
    CHECK(a.rows == 6 && a.cols == 1);
    CHECK(memcmp(a.elements.data, data, sizeof(data)) == 0);
    matrix_destroy(&a);
    matrix_destroy(&result);
    return true;
}

static bool test_matrix_determinant(void)
{
    struct matrix matrix;
    matrix_init(&matrix);
    const struct {
        size_t size;
        double data[9];
        double expected;
    } cases[] = {
        { 1, { -7 }, -7 },
        { 2, { 0, 2, 3, 4 }, -6 },
        { 2, { 1, 2, 2, 4 }, 0 },
        { 2, { 0, 0, 0, 0 }, 0 },
        { 3, { 6, 1, 1, 4, -2, 5, 2, 8, 7 }, -306 },
        { 2, { 0, 1e200, 1e-200, 0 }, -1 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        size_t n = cases[i].size;
        double result = 123;
        CHECK(matrix_assign(&matrix, n, n, cases[i].data));
        CHECK(matrix_det(&matrix, &result));
        CHECK(fabs(result - cases[i].expected) < 1e-12);
        CHECK(memcmp(matrix.elements.data, cases[i].data, n * n * sizeof(double)) == 0);
    }
    matrix_destroy(&matrix);
    return true;
}

static bool test_matrix_eigenvalues(void)
{
    struct matrix matrix;
    matrix_init(&matrix);
    VEC(values, struct matrix_eigenvalue);
    const struct {
        size_t size;
        double data[16];
        struct matrix_eigenvalue expected[4];
        double tolerance;
    } cases[] = {
        { 1, { -3 }, { { -3, 0 } }, 1e-12 },
        { 2, { 2, 1, 1, 2 }, { { 1, 0 }, { 3, 0 } }, 1e-12 },
        { 2, { 0, -1, 1, 0 }, { { 0, 1 }, { 0, -1 } }, 1e-12 },
        { 2, { 0, 0, 0, 0 }, { { 0, 0 }, { 0, 0 } }, 1e-12 },
        { 3, { 0, 0, 6, 1, 0, -11, 0, 1, 6 },
          { { 1, 0 }, { 2, 0 }, { 3, 0 } }, 1e-10 },
        { 3, { 0, 0, 2, 1, 0, -1, 0, 1, 2 },
          { { 2, 0 }, { 0, 1 }, { 0, -1 } }, 1e-10 },
        { 3, { 2, 4, -1, 0, -3, 2, 0, 0, 5 },
          { { 2, 0 }, { -3, 0 }, { 5, 0 } }, 1e-12 },
        { 3, { 2, 0, 0, 0, 2, 0, 0, 0, -1 },
          { { 2, 0 }, { 2, 0 }, { -1, 0 } }, 1e-12 },
        /* Similar to diag(6, 18) combined with [0, -12; 12, 0]. */
        { 4, { 9, 17, -5, 1, 3, 9, -9, 3, -21, 27, -3, 15, -3, 15, -3, 9 },
          { { 6, 0 }, { 18, 0 }, { 0, 12 }, { 0, -12 } }, 1e-10 },
        /* Similar to diag(6, 12, 18, 24). */
        { 4, { 21, -7, 7, -11, -3, 17, 1, 1, 12, -8, 26, -16, 12, -8, 8, -4 },
          { { 6, 0 }, { 12, 0 }, { 18, 0 }, { 24, 0 } }, 1e-10 },
        { 2, { 0, -1e-200, 1e-200, 0 },
          { { 0, 1e-200 }, { 0, -1e-200 } }, 1e-212 },
        { 2, { 0, -1e200, 1e200, 0 },
          { { 0, 1e200 }, { 0, -1e200 } }, 1e188 },
        { 2, { 1, 1e300, 1e-300, 1 }, { { 0, 0 }, { 2, 0 } }, 1e-12 },
    };
    for (size_t i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        size_t n = cases[i].size;
        CHECK(matrix_assign(&matrix, n, n, cases[i].data));
        CHECK(matrix_eigenvalues(&matrix, &values));
        CHECK(values.size == n);
        bool matched[4] = { false };
        for (size_t j = 0; j < n; ++j) {
            bool found = false;
            const struct matrix_eigenvalue *value = vec_at_const(&values, j);
            for (size_t k = 0; k < n; ++k) {
                if (!matched[k] &&
                    fabs(value->real - cases[i].expected[k].real) < cases[i].tolerance &&
                    fabs(value->imag - cases[i].expected[k].imag) < cases[i].tolerance) {
                    matched[k] = true;
                    found = true;
                    break;
                }
            }
            CHECK(found);
        }
        CHECK(memcmp(matrix.elements.data, cases[i].data, n * n * sizeof(double)) == 0);
    }
    matrix_destroy(&matrix);
    vec_destroy(&values);
    return true;
}

static bool test_matrix_inverse(void)
{
    struct matrix matrix, inverse, product;
    matrix_init(&matrix);
    matrix_init(&inverse);
    matrix_init(&product);

    double data[] = { 4, 7, 2, 6 };
    double expected[] = { 0.6, -0.7, -0.2, 0.4 };
    double identity[] = { 1, 0, 0, 1 };
    CHECK(matrix_assign(&matrix, 2, 2, data));
    CHECK(matrix_inverse(&inverse, &matrix));
    for (size_t i = 0; i < 4; ++i) {
        CHECK(fabs(((double *)inverse.elements.data)[i] - expected[i]) < 1e-12);
    }
    CHECK(matrix_mul(&product, &matrix, &inverse));
    for (size_t i = 0; i < 4; ++i) {
        CHECK(fabs(((double *)product.elements.data)[i] - identity[i]) < 1e-12);
    }
    CHECK(matrix_inverse(&matrix, &matrix));
    for (size_t i = 0; i < 4; ++i) {
        CHECK(fabs(((double *)matrix.elements.data)[i] - expected[i]) < 1e-12);
    }

    double three_by_three[] = { 1, 2, 3, 0, 1, 4, 5, 6, 0 };
    double three_inverse[] = { -24, 18, 5, 20, -15, -4, -5, 4, 1 };
    CHECK(matrix_assign(&matrix, 3, 3, three_by_three));
    CHECK(matrix_inverse(&inverse, &matrix));
    for (size_t i = 0; i < 9; ++i) {
        CHECK(fabs(((double *)inverse.elements.data)[i] - three_inverse[i]) < 1e-12);
    }

    double singular[] = { 1, 2, 2, 4 };
    double retained[] = { 31, 32, 33, 34 };
    CHECK(matrix_assign(&matrix, 2, 2, singular));
    CHECK(matrix_assign(&inverse, 2, 2, retained));
    CHECK(!matrix_inverse(&inverse, &matrix));
    CHECK(memcmp(inverse.elements.data, retained, sizeof(retained)) == 0);
    CHECK(matrix_assign(&matrix, 2, 3, three_by_three));
    CHECK(!matrix_inverse(&inverse, &matrix));
    CHECK(memcmp(inverse.elements.data, retained, sizeof(retained)) == 0);

    matrix_destroy(&matrix);
    matrix_destroy(&inverse);
    matrix_destroy(&product);
    return true;
}

static bool test_matrix_error_outputs(void)
{
    struct matrix a, b, output, empty;
    matrix_init(&a);
    matrix_init(&b);
    matrix_init(&output);
    matrix_init(&empty);
    double data[] = { 1, 2, 3, 4, 5, 6 };
    double sentinel[] = { 91, 92 };
    CHECK(matrix_assign(&a, 2, 3, data));
    CHECK(matrix_assign(&b, 2, 2, data));
    CHECK(matrix_assign(&output, 1, 2, sentinel));
    CHECK(!matrix_resize(&a, 0, 2));
    CHECK(!matrix_resize(&a, 2, 0));
    CHECK(!matrix_resize(&a, SIZE_MAX, 2));
    CHECK(!matrix_resize(&a, SIZE_MAX / sizeof(double) + 1, 1));
    CHECK(!matrix_assign(&a, 0, 2, data));
    CHECK(a.rows == 2 && a.cols == 3);
    CHECK(memcmp(a.elements.data, data, sizeof(data)) == 0);
    CHECK(!matrix_add(&output, &a, &b));
    CHECK(!matrix_sub(&output, &a, &b));
    CHECK(!matrix_mul(&output, &a, &b));
    CHECK(!matrix_transpose(&output, &empty));
    CHECK(!matrix_add(&output, &empty, &empty));
    CHECK(!matrix_mul(&output, &empty, &empty));
    CHECK(output.rows == 1 && output.cols == 2);
    CHECK(memcmp(output.elements.data, sentinel, sizeof(sentinel)) == 0);
    double determinant = 123;
    CHECK(!matrix_det(&a, &determinant));
    CHECK(!matrix_det(&empty, &determinant));
    CHECK(determinant == 123);
    VEC(values, struct matrix_eigenvalue);
    struct matrix_eigenvalue root = { 4, 5 };
    CHECK(vec_push_back(&values, &root));
    CHECK(!matrix_eigenvalues(&a, &values));
    CHECK(!matrix_eigenvalues(&empty, &values));
    CHECK(values.size == 1);
    const struct matrix_eigenvalue *retained = vec_at_const(&values, 0);
    CHECK(retained->real == 4 && retained->imag == 5);
    matrix_destroy(&a);
    matrix_destroy(&b);
    matrix_destroy(&output);
    matrix_destroy(&empty);
    vec_destroy(&values);
    return true;
}

struct test_case {
    const char *name;
    bool (*function)(void);
};

int main(void)
{
    const struct test_case tests[] = {
        { "vec lifecycle and access", test_vec_lifecycle_and_access },
        { "vec insert, erase and assign", test_vec_insert_erase_and_assign },
        { "vec resize, capacity and swap", test_vec_resize_capacity_and_swap },
        { "vec iterators and comparison", test_vec_iterators_and_comparison },
        { "list and container_of", test_list_and_container_of },
        { "poly_vec normalization", test_poly_vec_normalization },
        { "poly_vec add, sub and alias", test_poly_vec_add_sub_and_alias },
        { "poly_vec multiply, derivative and eval",
          test_poly_vec_mul_derivative_and_eval },
        { "poly_list normalization and conversion",
          test_poly_list_normalization_and_conversion },
        { "poly_list arithmetic and alias",
          test_poly_list_arithmetic_and_alias },
        { "expression nested precedence and variables",
          test_expression_nested_precedence_and_variables },
        { "expression variable dedup and update",
          test_expression_variable_dedup_and_update },
        { "expression unary and decimal values",
          test_expression_unary_and_decimal_values },
        { "expression deep parentheses and associativity",
          test_expression_deep_parentheses_and_associativity },
        { "expression error paths", test_expression_error_paths },
        { "expression power precedence", test_expression_power_precedence },
        { "expression scientific literals and variables",
          test_expression_scientific_literals_and_variables },
        { "expression comparisons", test_expression_comparisons },
        { "expression conditional nesting", test_expression_conditional_nesting },
        { "expression conditional short circuit", test_expression_conditional_short_circuit },
        { "expression function calls", test_expression_function_calls },
        { "expression recursive calls", test_expression_recursive_calls },
        { "expression function history", test_expression_function_history },
        { "expression definition errors", test_expression_definition_errors },
        { "expression conditional and call errors",
          test_expression_conditional_and_call_errors },
        { "matrix storage", test_matrix_storage },
        { "matrix add, sub and alias", test_matrix_add_sub_and_alias },
        { "matrix multiply and alias", test_matrix_multiply_and_alias },
        { "matrix transpose", test_matrix_transpose },
        { "matrix determinant", test_matrix_determinant },
        { "matrix eigenvalues", test_matrix_eigenvalues },
        { "matrix inverse", test_matrix_inverse },
        { "matrix error outputs", test_matrix_error_outputs },
        { "polynomial integral", test_polynomial_integral },
        { "polynomial integral edge cases", test_polynomial_integral_edges },
        { "vec mutation edges", test_vec_mutation_edges },
        { "list replace and move", test_list_replace_and_move },
        { "polynomial copy and derivative edges",
          test_polynomial_copy_and_derivative_edges },
    };
    size_t failures = 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("[%zu/%zu] %s\n", i + 1,
               sizeof(tests) / sizeof(tests[0]), tests[i].name);
        if (tests[i].function()) {
            puts("  passed");
        } else {
            ++failures;
            puts("  failed");
        }
    }
    printf("\n%zu/%zu tests passed\n",
           sizeof(tests) / sizeof(tests[0]) - failures,
           sizeof(tests) / sizeof(tests[0]));
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
