/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

#ifndef STACK_H
#define STACK_H

/**
 * @file stack.h
 * @brief LIFO stack primitives backed by a dynamically sized vector.
 *
 * A stack stores elements by value and uses the last vector element as its
 * top.  The element size is fixed when the stack is initialized.
 */

#include "vec.h"

/**
 * @brief A generic last-in, first-out stack.
 */
typedef struct stack {
    /** @brief Contiguous storage for the stack elements. */
    vec container;
} stack;

/** @name Initialization */
/** @{ */

/**
 * @brief Produces an initializer for an empty stack of the given type.
 * @param type The element type.
 * @return An initializer for a `stack` object.
 */
#define STACK_INIT(type) { \
    .container = VEC_INIT(type), \
}

/**
 * @brief Declares and initializes an empty stack.
 * @param name The stack variable name.
 * @param type The element type.
 */
#define STACK(name, type) \
    stack name = STACK_INIT(type)

/**
 * @brief Initializes an existing stack for elements of `type`.
 * @param s A pointer to the stack.
 * @param type The element type.
 * @post `s` is empty and its element size is `sizeof(type)`.
 */
#define stack_init(s, type) \
    vec_init(&(s)->container, type)

/**
 * @brief Destroys a stack and releases its allocated storage.
 * @param s A pointer to the stack.
 * @pre `s` points to a stack initialized by `STACK` or `stack_init`.
 */
static inline void stack_destroy(stack *s)
{
    vec_destroy(&s->container);
}

/** @} */

/** @name Basic properties */
/** @{ */

/**
 * @brief Returns the number of elements in the stack.
 * @param s A pointer to the stack.
 * @return The number of elements in the stack.
 * @pre `s` points to an initialized stack.
 */
static inline size_t stack_size(const stack *s)
{
    return vec_size(&s->container);
}

/**
 * @brief Tests whether the stack is empty.
 * @param s A pointer to the stack.
 * @return `true` when the stack contains no elements; otherwise `false`.
 * @pre `s` points to an initialized stack.
 */
static inline bool stack_empty(const stack *s)
{
    return vec_empty(&s->container);
}

/**
 * @brief Removes all elements without releasing the allocated capacity.
 * @param s A pointer to the stack.
 * @post The stack is empty and its allocated capacity is unchanged.
 * @pre `s` points to an initialized stack.
 */
static inline void stack_clear(stack *s)
{
    vec_clear(&s->container);
}

/**
 * @brief Exchanges the contents of two stacks.
 * @param a A pointer to the first stack.
 * @param b A pointer to the second stack.
 * @pre Both stacks are initialized with the same element size.
 */
static inline void stack_swap(stack *a, stack *b)
{
    vec_swap(&a->container, &b->container);
}

/** @} */

/** @name Top element */
/** @{ */

/**
 * @brief Returns a writable pointer to the top element.
 * @param s A pointer to the stack.
 * @return A pointer to the top element.
 * @pre `s` points to an initialized stack containing at least one element.
 */
static inline void *stack_top(stack *s)
{
    return vec_back(&s->container);
}

/**
 * @brief Returns a read-only pointer to the top element.
 * @param s A pointer to the stack.
 * @return A pointer to the top element.
 * @pre `s` points to an initialized stack containing at least one element.
 */
static inline const void *stack_top_const(const stack *s)
{
    return vec_back_const(&s->container);
}

/** @} */

/** @name Insertion and erasure */
/** @{ */

/**
 * @brief Pushes one raw element onto the stack.
 * @param s A pointer to the stack.
 * @param value A pointer to the element to copy.
 * @return `true` on success; `false` for invalid input or allocation failure.
 * @pre `s` points to an initialized stack.
 */
static inline bool stack_push(stack *s, const void *value)
{
    return vec_push_back(&s->container, value);
}

/**
 * @brief Pushes a typed value after checking its size.
 * @param s A pointer to the stack.
 * @param value The value to copy onto the stack.
 * @pre `s` points to an initialized stack whose element type is `typeof(value)`.
 */
#define stack_push_value(s, value) ({                 \
    typeof(value) __stack_tmp = (value);              \
    assert(sizeof(__stack_tmp) ==                     \
           (s)->container.elem_size);                \
    stack_push((s), &__stack_tmp);                    \
})

/**
 * @brief Removes the top element, if one exists.
 * @param s A pointer to the stack.
 *
 * Like `std::stack::pop`, this function does not return the removed value.
 * @pre `s` points to an initialized stack.
 */
static inline void stack_pop(stack *s)
{
    vec_pop_back(&s->container);
}

/**
 * @brief Removes the top element and copies it to `out`.
 * @param s A pointer to the stack.
 * @param out A pointer receiving the removed element.
 * @return `true` on success; `false` when the stack is empty or invalid.
 * @pre `s` points to an initialized stack.
 */
static inline bool stack_pop_copy(stack *s, void *out)
{
    return vec_pop_back_copy(&s->container, out);
}

/** @} */

/** @name Typed access helpers */
/** @{ */

/**
 * @brief Returns the top element converted to a writable typed pointer.
 * @param s A pointer to the stack.
 * @param type The element type.
 * @pre `s` points to an initialized, non-empty stack.
 */
#define stack_top_as(s, type) \
    ((type *)stack_top((s)))

/**
 * @brief Returns the top element converted to a read-only typed pointer.
 * @param s A pointer to the stack.
 * @param type The element type.
 * @pre `s` points to an initialized, non-empty stack.
 */
#define stack_top_as_const(s, type) \
    ((const type *)stack_top_const((s)))

/** @} */

#endif /* STACK_H */
