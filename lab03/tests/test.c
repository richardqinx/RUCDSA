/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#include "container_of.h"
#include "list.h"
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
    CHECK(!vec_reserve(&vector, SIZE_MAX));

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
    };
    size_t failures = 0;

    for (size_t i = 0; i < sizeof(tests) / sizeof(tests[0]); ++i) {
        printf("[%zu/10] %s\n", i + 1, tests[i].name);
        if (tests[i].function()) {
            puts("  passed");
        } else {
            ++failures;
            puts("  failed");
        }
    }
    printf("\n%zu/10 tests passed\n",
           sizeof(tests) / sizeof(tests[0]) - failures);
    return failures == 0 ? EXIT_SUCCESS : EXIT_FAILURE;
}
