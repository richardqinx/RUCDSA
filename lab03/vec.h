/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Copyright (C) 2026 Richard Qin */

/**
 * @file vec.h
 * @brief A dynamically sized contiguous sequence with byte-based storage.
 *
 * `struct vec` stores the element size rather than a type.  The caller is
 * responsible for using the type associated with the initialized element size.
 */
#ifndef VEC_H
#define VEC_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>

/**
 * @brief A dynamically sized contiguous sequence.
 *
 * `data` points to storage for at most `capacity` elements.  The first
 * `size` positions contain valid elements.
 */
struct vec {
    void *data;
    size_t size;
    size_t capacity;
    size_t elem_size;
};

/**
 * @brief Compares two elements.
 *
 * @return A value less than, equal to, or greater than zero when the left
 * element is less than, equal to, or greater than the right element.
 */
typedef int (*vec_compare_fn)(const void *lhs, const void *rhs);

/** @name Initialization */
/** @{ */

/**
 * @brief Produces an initializer for an empty vector of the given element type.
 * @param type The element type.
 */
#define VEC_INIT(type) {          \
    .data = NULL,                 \
    .size = 0,                    \
    .capacity = 0,                \
    .elem_size = sizeof(type),    \
}

/**
 * @brief Declares and initializes a vector for the given element type.
 * @param name The vector variable name.
 * @param type The element type.
 */
#define VEC(name, type) \
  struct vec name = VEC_INIT(type)

/**
 * @brief Initializes an existing vector.
 * @param v A pointer to the vector.
 * @param type The element type.
 */
#define vec_init(v, type) \
    (*(v) = (struct vec)VEC_INIT(type))

/** @} */

/** @name Basic properties */
/** @{ */

/** @brief Returns the number of valid elements in the vector. */
static inline size_t vec_size(const struct vec *v)
{
    return v->size;
}

/** @brief Returns the current storage capacity of the vector. */
static inline size_t vec_capacity(const struct vec *v)
{
    return v->capacity;
}

/** @brief Returns the size of one element in bytes. */
static inline size_t vec_elem_size(const struct vec *v)
{
    return v->elem_size;
}

/**
 * @brief Returns the maximum number of elements representable at this element size.
 */
static inline size_t vec_max_size(const struct vec *v)
{
    return SIZE_MAX / v->elem_size;
}

/** @brief Tests whether the vector is empty. */
static inline int vec_empty(const struct vec *v)
{
    return v->size == 0;
}

/** @} */

/** @name Lifetime and capacity */
/** @{ */

/**
 * @brief Removes all valid elements from the vector.
 *
 * The operation does not release the allocated capacity.
 */
static inline void vec_clear(struct vec *v)
{
    v->size = 0;
}

/**
 * @brief Destroys the vector and releases its storage.
 */
static inline void vec_destroy(struct vec *v)
{
    free(v->data);

    v->data = NULL;
    v->size = 0;
    v->capacity = 0;
    v->elem_size = 0;
}

/**
 * @brief Ensures that the vector has at least the requested capacity.
 * @param v A pointer to the vector.
 * @param capacity The minimum requested number of elements.
 * @return `true` on success; `false` on overflow or allocation failure.
 */
static inline bool vec_reserve(struct vec *v, size_t capacity)
{
    if (!v || v->elem_size == 0) {
        return false;
    }
    // 当前容量已满足需求，直接返回成功
    if (capacity <= v->capacity) {
        return true;
    }
    // 乘法溢出检查 (capacity * elem_size > SIZE_MAX)
    if (capacity > SIZE_MAX / v->elem_size) {
        return false;
    }

    size_t new_bytes = capacity * v->elem_size;
    void *new_data = realloc(v->data, new_bytes);
    if (!new_data) {
        return false; // 分配失败，原 v->data 依然有效
    }

    v->data = new_data;
    v->capacity = capacity;
    return true;
}

/**
 * @brief Changes the number of valid elements in the vector.
 *
 * Elements added by growing the vector are uninitialized.
 *
 * @param v A pointer to the vector.
 * @param size The new number of elements.
 * @return `true` on success; `false` on allocation failure.
 */
static inline bool vec_resize(struct vec *v, size_t size)
{
    if (!v) {
        return false;
    }

    if (size > v->capacity) {
        if (!vec_reserve(v, size)) {
            return false;
        }
    }
    v->size = size;
    return true;
}

/**
 * @brief Changes the vector size and clears newly added bytes.
 *
 * Clearing bytes does not guarantee a type-specific zero value; it only
 * guarantees that the corresponding storage bytes are zero.
 */
static inline bool vec_resize_zero(struct vec *v, size_t size)
{
    if (!v) {
        return false;
    }
    size_t old_size = v->size;

    if (size > old_size) {
        if (size > v->capacity) {
            if (!vec_reserve(v, size)) {
                return false;
            }
        }
        /* 这里使用 memset 清零字节，不保证所有元素类型都得到
         * 该类型语义上的零值。 */
        char *start = (char *)v->data + (old_size * v->elem_size);
        size_t zero_bytes = (size - old_size) * v->elem_size;
        memset(start, 0, zero_bytes);
    }

    v->size = size;
    return true;
}

/**
 * @brief Attempts to reduce the capacity to the current size.
 * @return `true` on success; `false` if reallocation fails.
 */
static inline bool vec_shrink_to_fit(struct vec *v)
{
  if (!v || v->elem_size == 0) {
        return false;
    }
    // 已经完全贴合，无需缩容
    if (v->capacity == v->size) {
        return true;
    }

    // 当 size == 0 时，彻底归还堆内存
    if (v->size == 0) {
        free(v->data);
        v->data = NULL;
        v->capacity = 0;
        return true;
    }

    // 缩容至刚好容纳 size 个元素
    size_t bytes = v->size * v->elem_size;
    void *new_data = realloc(v->data, bytes);
    if (!new_data) {
        return false; // 极少数情况下缩容可能失败，保持原样
    }

    v->data = new_data;
    v->capacity = v->size;
    return true;
}

/** @} */

/** @name Raw element access */
/** @{ */

/** @brief Returns a writable pointer to the vector storage. */
static inline void *vec_data(struct vec *v)
{
    return v->data;
}

/** @brief Returns a read-only pointer to the vector storage. */
static inline const void *vec_data_const(const struct vec *v)
{
    return v->data;
}

/**
 * @brief Tests two vectors for equality using a comparison function.
 */
static inline bool vec_equal(const struct vec *a,
                             const struct vec *b,
                             vec_compare_fn compare)
{
    if (a->elem_size != b->elem_size || a->size != b->size) {
        return false;
    }

    for (size_t i = 0; i < a->size; ++i) {
        const void *lhs = (const unsigned char *)a->data
                        + i * a->elem_size;
        const void *rhs = (const unsigned char *)b->data
                        + i * b->elem_size;
        if (compare(lhs, rhs) != 0) {
            return false;
        }
    }
    return true;
}

/**
 * @brief Compares two vectors lexicographically.
 * @return -1, 0, or 1 when the left vector is less than, equal to, or
 * greater than the right vector.
 */
static inline int vec_compare(const struct vec *a,
                              const struct vec *b,
                              vec_compare_fn compare)
{
    size_t common_size = a->size < b->size ? a->size : b->size;

    for (size_t i = 0; i < common_size; ++i) {
        const void *lhs = (const unsigned char *)a->data
                        + i * a->elem_size;
        const void *rhs = (const unsigned char *)b->data
                        + i * b->elem_size;
        int result = compare(lhs, rhs);
        if (result < 0) {
            return -1;
        }
        if (result > 0) {
            return 1;
        }
    }

    if (a->size < b->size) {
        return -1;
    }
    if (a->size > b->size) {
        return 1;
    }
    return 0;
}

/**
 * @brief Returns a writable pointer to the element at `index`.
 * @pre `index < v->size`.
 */
static inline void *vec_at(struct vec *v, size_t index)
{
    assert(index < v->size);

    return (unsigned char *)v->data
         + index * v->elem_size;
}

/**
 * @brief Returns a read-only pointer to the element at `index`.
 * @pre `index < v->size`.
 */
static inline const void *vec_at_const(const struct vec *v,
                                       size_t index)
{
    assert(index < v->size);

    return (const unsigned char *)v->data
         + index * v->elem_size;
}

/**
 * @brief Returns a writable pointer to the element at `index`, or `NULL`
 * when `index` is out of range.
 */
static inline void *vec_at_checked(struct vec *v, size_t index)
{
    if (index >= v->size) {
        return NULL;
    }

    return (unsigned char *)v->data
         + index * v->elem_size;
}

/**
 * @brief Returns a read-only pointer to the element at `index`, or `NULL`
 * when `index` is out of range.
 */
static inline const void *vec_at_checked_const(const struct vec *v,
                                               size_t index)
{
    if (index >= v->size) {
        return NULL;
    }

    return (const unsigned char *)v->data
         + index * v->elem_size;
}

/**
 * @brief Returns a writable pointer to the first element.
 * @pre The vector is not empty.
 */
static inline void *vec_front(struct vec *v)
{
    assert(v->size);
    return v->data;
}

/**
 * @brief Returns a read-only pointer to the first element.
 * @pre The vector is not empty.
 */
static inline const void *vec_front_const(const struct vec *v)
{
    assert(v->size);
    return v->data;
}

/**
 * @brief Returns a writable pointer to the last element.
 * @pre The vector is not empty.
 */
static inline void *vec_back(struct vec *v)
{
    assert(v->size);
    return vec_at(v, v->size - 1);
}

/**
 * @brief Returns a read-only pointer to the last element.
 * @pre The vector is not empty.
 */
static inline const void *vec_back_const(const struct vec *v)
{
    assert(v->size);
    return vec_at_const(v, v->size - 1);
}

/** @} */

/** @name Typed access macros */
/** @{ */

/** @brief Converts the storage pointer to a writable pointer of `type`. */
#define vec_data_as(v, type) \
    ((type *)vec_data(v))

/** @brief Converts the storage pointer to a read-only pointer of `type`. */
#define vec_data_as_const(v, type) \
    ((const type *)vec_data_const(v))

/** @brief Accesses the writable element at `i` as `type`. */
#define vec_at_as(v, type, i) \
    (*(type *)vec_at((v), (i)))

/** @brief Accesses the read-only element at `i` as `type`. */
#define vec_at_as_const(v, type, i) \
    (*(const type *)vec_at_const((v), (i)))

/** @brief Accesses the first element as a writable `type`. */
#define vec_front_as(v, type) \
    (*(type *)vec_front(v))

/** @brief Accesses the first element as a read-only `type`. */
#define vec_front_as_const(v, type) \
    (*(const type *)vec_front_const(v))

/** @brief Accesses the last element as a writable `type`. */
#define vec_back_as(v, type) \
    (*(type *)vec_back(v))

/** @brief Accesses the last element as a read-only `type`. */
#define vec_back_as_const(v, type) \
    (*(const type *)vec_back_const(v))

/** @} */

/** @name Raw element insertion */
/** @{ */

/* 内部辅助函数：按 2 倍几何级数扩容，确保至少容纳 needed 个元素 */
static inline bool __vec_grow_to(struct vec *v, size_t needed)
{
    size_t max_elements = SIZE_MAX / v->elem_size;
    if (needed > max_elements) {
        return false;
    }
    if (needed <= v->capacity) {
        return true;
    }
    // 初始默认给 8 个元素容量，之后每次翻倍
    size_t new_cap = v->capacity ? v->capacity : 8;
    if (new_cap > max_elements) {
        new_cap = max_elements;
    }
    while (new_cap < needed) {
        // 防止翻倍超过最大元素数量
        if (new_cap > max_elements / 2) {
            new_cap = max_elements;
            break;
        }
        new_cap *= 2;
    }
    return vec_reserve(v, new_cap);
}

/**
 * @brief Reserves an uninitialized slot at the end of the vector.
 * @return A pointer to the new slot, or `NULL` on failure.
 */
static inline void *vec_emplace_back(struct vec *v)
{
    if (!v || v->elem_size == 0) {
        return NULL;
    }
    size_t max_elements = SIZE_MAX / v->elem_size;
    if (v->size >= max_elements) {
        return NULL;
    }
    size_t needed = v->size + 1;
    /* Grow the storage when the current capacity is exhausted. */
    if (v->size >= v->capacity) {
        if (!__vec_grow_to(v, needed)) {
            return NULL;
        }
    }
    void *slot = (char *)v->data + (v->size * v->elem_size);
    v->size++;
    return slot;
}

static inline size_t __vec_source_index(const struct vec *v,
                                        const void *elem)
{
    if (!v || !v->data || !elem || v->elem_size == 0) {
        return SIZE_MAX;
    }

    uintptr_t base = (uintptr_t)v->data;
    uintptr_t address = (uintptr_t)elem;
    if (address < base) {
        return SIZE_MAX;
    }

    uintptr_t offset = address - base;
    if (offset % v->elem_size != 0) {
        return SIZE_MAX;
    }

    size_t index = (size_t)(offset / v->elem_size);
    return index < v->size ? index : SIZE_MAX;
}

/**
 * @brief Appends one raw element to the vector.
 * @param v A pointer to the vector.
 * @param elem A pointer to the element to copy.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_push_back(struct vec *v,
                                 const void *elem)
{
    if (!elem) {
        return false;
    }
    size_t source_index = __vec_source_index(v, elem);
    void *slot = vec_emplace_back(v);
    if (!slot) {
        return false;
    }
    const void *source = source_index == SIZE_MAX
        ? elem
        : (const unsigned char *)v->data
          + source_index * v->elem_size;
    memcpy(slot, source, v->elem_size);
    return true;
}

/**
 * @brief Reserves an uninitialized slot at `index`.
 * @param v A pointer to the vector.
 * @param index The insertion index in the range `[0, size]`.
 * @return A pointer to the new slot, or `NULL` on failure.
 */
static inline void *vec_emplace(struct vec *v,
                                size_t index)
{
    if (!v || v->elem_size == 0 || index > v->size) {
        return NULL;
    }
    size_t max_elements = SIZE_MAX / v->elem_size;
    if (v->size >= max_elements) {
        return NULL;
    }
    size_t needed = v->size + 1;
    if (v->size >= v->capacity) {
        if (!__vec_grow_to(v, needed)) {
            return NULL;
        }
    }
    char *slot = (char *)v->data + (index * v->elem_size);

    /* Shift the suffix when insertion is not at the end. */
    if (index < v->size) {
        memmove(slot + v->elem_size, slot, (v->size - index) * v->elem_size);
    }

    v->size++;
    return (void *)slot;

}

/**
 * @brief Inserts one raw element at `index`.
 * @param v A pointer to the vector.
 * @param index The insertion index in the range `[0, size]`.
 * @param elem A pointer to the element to copy.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_insert(struct vec *v,
                              size_t index,
                              const void *elem)
{
    if (!elem) {
        return false;
    }
    size_t source_index = __vec_source_index(v, elem);
    void *slot = vec_emplace(v, index);
    if (!slot) {
        return false;
    }
    if (source_index != SIZE_MAX && source_index >= index) {
        source_index++;
    }
    const void *source = source_index == SIZE_MAX
        ? elem
        : (const unsigned char *)v->data
          + source_index * v->elem_size;
    memcpy(slot, source, v->elem_size);
    return true;
}

/**
 * @brief Inserts a range of raw elements at `index`.
 *
 * The source range must not refer to the vector's storage.
 *
 * @param v A pointer to the vector.
 * @param index The insertion index in the range `[0, size]`.
 * @param src The source range.
 * @param count The number of elements to insert.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_insert_range(struct vec *v,
                                    size_t index,
                                    const void *src,
                                    size_t count)
{
    /* 前置条件：src 不得指向当前 vector 的存储区，
     * 这与 std::vector::insert(position, first, last) 一致。 */
    if (!v || v->elem_size == 0 || index > v->size) {
        return false;
    }
    if (count == 0) {
        return true; /* Inserting zero elements succeeds. */
    }
    if (!src) {
        return false;
    }
    /* Check for overflow in size + count. */
    if (count > SIZE_MAX - v->size) {
        return false;
    }

    size_t needed = v->size + count;
    if (needed > v->capacity) {
        if (!__vec_grow_to(v, needed)) {
            return false;
        }
    }

    char *dest = (char *)v->data + (index * v->elem_size);

    /* Shift the suffix when insertion is not at the end. */
    if (index < v->size) {
        memmove(dest + (count * v->elem_size), dest, (v->size - index) * v->elem_size);
    }

    /* Copy the source range into the newly opened gap. */
    memcpy(dest, src, count * v->elem_size);
    v->size += count;
    return true;
}

/**
 * @brief Replaces the vector with `count` copies of one raw element.
 * @param v A pointer to the vector.
 * @param count The resulting number of elements.
 * @param value A pointer to the value to copy.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_assign_n(struct vec *v,
                                size_t count,
                                const void *value)
{
    if (!v || v->elem_size == 0) {
        return false;
    }
    if (count == 0) {
        v->size = 0;
        return true;
    }
    if (!value || count > vec_max_size(v)) {
        return false;
    }

    size_t source_index = __vec_source_index(v, value);
    if (count > v->capacity && !vec_reserve(v, count)) {
        return false;
    }

    const void *source = source_index == SIZE_MAX
        ? value
        : (const unsigned char *)v->data
          + source_index * v->elem_size;
    for (size_t i = 0; i < count; ++i) {
        void *dest = (unsigned char *)v->data + i * v->elem_size;
        memmove(dest, source, v->elem_size);
    }
    v->size = count;
    return true;
}

/**
 * @brief Replaces the vector with a range of raw elements.
 *
 * The source range must not refer to the vector's storage.
 *
 * @param v A pointer to the vector.
 * @param src The source range.
 * @param count The number of elements to assign.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_assign_range(struct vec *v,
                                    const void *src,
                                    size_t count)
{
    /* 前置条件：src 不得指向当前 vector 的存储区。 */
    if (!v || v->elem_size == 0) {
        return false;
    }
    if (count == 0) {
        v->size = 0;
        return true;
    }
    if (!src || count > vec_max_size(v)) {
        return false;
    }
    if (count > v->capacity && !vec_reserve(v, count)) {
        return false;
    }

    memmove(v->data, src, count * v->elem_size);
    v->size = count;
    return true;
}

/**
 * @brief Appends a range of raw elements to the vector.
 *
 * The source range must not refer to the vector's storage.
 *
 * @param v A pointer to the vector.
 * @param src The source range.
 * @param count The number of elements to append.
 * @return `true` on success; `false` on invalid input or allocation failure.
 */
static inline bool vec_append(struct vec *v,
                              const void *src,
                              size_t count)
{
    /* The source must not refer to the vector's storage. */
  if (!v) {
        return false;
    }
    /* Reuse vec_insert_range() at the current end position. */
    return vec_insert_range(v, v->size, src, count);
}


/** @} */

/** @name Value insertion wrappers */
/** @{ */

/** @brief Appends a typed value after checking its size. */
#define vec_push(v, value) ({                 \
    typeof(value) __vec_tmp = (value);        \
    assert(sizeof(__vec_tmp) ==               \
           (v)->elem_size);                   \
    vec_push_back((v), &__vec_tmp);           \
})

/** @brief Inserts a typed value after checking its size. */
#define vec_insert_value(v, index, value) ({   \
    typeof(value) __vec_tmp = (value);        \
    assert(sizeof(__vec_tmp) ==               \
           (v)->elem_size);                   \
    vec_insert((v), (index), &__vec_tmp);     \
})

/** @brief Reserves a typed slot at the end of the vector. */
#define vec_emplace_back_as(v, type) \
    ((type *)vec_emplace_back(v))

/** @brief Reserves a typed slot at the specified index. */
#define vec_emplace_as(v, type, index) \
    ((type *)vec_emplace((v), (index)))


/** @} */

/** @name Erasure */
/** @{ */

/** @brief Removes the last element, if one exists. */
static inline void vec_pop_back(struct vec *v)
{
    if (!v || v->size == 0) {
        return;
    }
    v->size--;
}

/**
 * @brief Removes the last element and copies it to `out`.
 * @return `true` on success; `false` when the vector is empty or `out` is null.
 */
static inline bool vec_pop_back_copy(struct vec *v,
                                     void *out)
{
    if (!v || !out || v->size == 0) {
        return false;
    }
    v->size--;
    const char *last_elem = (const char *)v->data + (v->size * v->elem_size);
    memcpy(out, last_elem, v->elem_size);
    return true;
}

/**
 * @brief Removes up to `count` elements starting at `index`.
 */
static inline void vec_erase_n(struct vec *v,
                               size_t index,
                               size_t count)
{
    if (!v || index >= v->size || count == 0) {
        return;
    }

    /* Limit the count without allowing index + count to overflow. */
    if (count > v->size - index) {
        count = v->size - index;
    }

    size_t trailing_elems = v->size - (index + count);
    if (trailing_elems > 0) {
        char *dest = (char *)v->data + (index * v->elem_size);
        const char *src = (const char *)v->data + ((index + count) * v->elem_size);
        /* The source and destination overlap, so use memmove. */
        memmove(dest, src, trailing_elems * v->elem_size);
    }

    v->size -= count;
}

/** @brief Removes the element at `index`, if it exists. */
static inline void vec_erase(struct vec *v,
                             size_t index)
{
    vec_erase_n(v, index, 1);
}

/**
 * @brief Removes the half-open range `[first, last)`.
 * @return A pointer to the element now at `first`, or the end pointer.
 */
static inline void *vec_erase_range(struct vec *v,
                                    size_t first,
                                    size_t last)
{
    if (!v || first > last || last > v->size) {
        return NULL;
    }

    vec_erase_n(v, first, last - first);
    if (first == v->size) {
        if (!v->data) {
            return NULL;
        }
        return (unsigned char *)v->data + v->size * v->elem_size;
    }
    return (unsigned char *)v->data + first * v->elem_size;
}

/** @brief Exchanges the contents of two vectors. */
static inline void vec_swap(struct vec *a, struct vec *b)
{
    struct vec tmp = *a;
    *a = *b;
    *b = tmp;
}


/**
 * @brief Removes an element by replacing it with the last element.
 *
 * The order of the remaining elements is not preserved.
 */
static inline void vec_swap_erase(struct vec *v,
                                  size_t index)
{
    if (!v || index >= v->size) {
        return;
    }

    size_t last_idx = v->size - 1;
    /* Move the last element over the erased element when necessary. */
    if (index < last_idx) {
        char *dest = (char *)v->data + (index * v->elem_size);
        const char *src = (const char *)v->data + (last_idx * v->elem_size);
        memcpy(dest, src, v->elem_size);
    }

    v->size--;
}

/** @} */

/** @name Forward iterators */
/** @{ */

/** @brief Returns a writable iterator to the first element. */
static inline void *vec_begin(struct vec *v)
{
    return v->data;
}

/** @brief Returns a read-only iterator to the first element. */
static inline const void *vec_cbegin(const struct vec *v)
{
    return v->data;
}

/** @brief Returns a writable iterator past the last element. */
static inline void *vec_end(struct vec *v)
{
    if (!v->data)
        return NULL;

    return (unsigned char *)v->data
         + v->size * v->elem_size;
}

/** @brief Returns a read-only iterator past the last element. */
static inline const void *vec_cend(const struct vec *v)
{
    if (!v->data)
        return NULL;

    return (const unsigned char *)v->data
         + v->size * v->elem_size;
}

/** @brief Returns the first element as a typed writable iterator. */
#define vec_begin_as(v, type) \
    ((type *)vec_begin(v))

/** @brief Returns the end as a typed writable iterator. */
#define vec_end_as(v, type) \
    ((type *)vec_end(v))

/** @brief Returns the first element as a typed read-only iterator. */
#define vec_cbegin_as(v, type) \
    ((const type *)vec_cbegin(v))

/** @brief Returns the end as a typed read-only iterator. */
#define vec_cend_as(v, type) \
    ((const type *)vec_cend(v))


/** @} */

/** @name Iterator helpers */
/** @{ */

/** @brief Advances a pointer iterator by one element. */
#define vec_iter_next(it) \
    (++(it))

/** @brief Moves a pointer iterator back by one element. */
#define vec_iter_prev(it) \
    (--(it))

/**
 * @brief Returns the distance between two pointer iterators.
 *
 * Equal iterators, including two null pointers, have distance zero.
 */
#define vec_iter_distance(first, last) ({       \
    __auto_type __vec_first = (first);          \
    __auto_type __vec_last = (last);            \
    __vec_first == __vec_last                   \
        ? 0                                     \
        : __vec_last - __vec_first;             \
})

/**
 * @brief A reverse iterator over mutable vector storage.
 */
struct vec_reverse_iterator {
    void *data;
    size_t size;
    size_t elem_size;
    size_t offset;
};

/**
 * @brief A reverse iterator over read-only vector storage.
 */
struct vec_const_reverse_iterator {
    const void *data;
    size_t size;
    size_t elem_size;
    size_t offset;
};

/** @brief Returns a reverse iterator to the last element. */
static inline struct vec_reverse_iterator vec_rbegin(struct vec *v)
{
    return (struct vec_reverse_iterator){
        .data = v->data,
        .size = v->size,
        .elem_size = v->elem_size,
        .offset = 0,
    };
}

/** @brief Returns a reverse iterator past the first element. */
static inline struct vec_reverse_iterator vec_rend(struct vec *v)
{
    return (struct vec_reverse_iterator){
        .data = v->data,
        .size = v->size,
        .elem_size = v->elem_size,
        .offset = v->size,
    };
}

/** @brief Returns a read-only reverse iterator to the last element. */
static inline struct vec_const_reverse_iterator
vec_crbegin(const struct vec *v)
{
    return (struct vec_const_reverse_iterator){
        .data = v->data,
        .size = v->size,
        .elem_size = v->elem_size,
        .offset = 0,
    };
}

/** @brief Returns a read-only reverse iterator past the first element. */
static inline struct vec_const_reverse_iterator
vec_crend(const struct vec *v)
{
    return (struct vec_const_reverse_iterator){
        .data = v->data,
        .size = v->size,
        .elem_size = v->elem_size,
        .offset = v->size,
    };
}

/** @brief Dereferences a mutable reverse iterator. */
static inline void *vec_reverse_deref(struct vec_reverse_iterator it)
{
    return (unsigned char *)it.data
         + (it.size - it.offset - 1) * it.elem_size;
}

/** @brief Dereferences a read-only reverse iterator. */
static inline const void *
vec_reverse_deref_const(struct vec_const_reverse_iterator it)
{
    return (const unsigned char *)it.data
         + (it.size - it.offset - 1) * it.elem_size;
}

/** @brief Advances a mutable reverse iterator. */
static inline void vec_reverse_next(struct vec_reverse_iterator *it)
{
    ++it->offset;
}

/** @brief Advances a read-only reverse iterator. */
static inline void
vec_reverse_next_const(struct vec_const_reverse_iterator *it)
{
    ++it->offset;
}

/** @brief Moves a mutable reverse iterator backwards. */
static inline void vec_reverse_prev(struct vec_reverse_iterator *it)
{
    --it->offset;
}

/** @brief Moves a read-only reverse iterator backwards. */
static inline void
vec_reverse_prev_const(struct vec_const_reverse_iterator *it)
{
    --it->offset;
}

/** @brief Compares two mutable reverse iterators. */
static inline bool vec_reverse_equal(struct vec_reverse_iterator lhs,
                                     struct vec_reverse_iterator rhs)
{
    return lhs.data == rhs.data && lhs.offset == rhs.offset;
}

/** @brief Compares two read-only reverse iterators. */
static inline bool
vec_reverse_equal_const(struct vec_const_reverse_iterator lhs,
                        struct vec_const_reverse_iterator rhs)
{
    return lhs.data == rhs.data && lhs.offset == rhs.offset;
}

/** @brief Returns the distance between mutable reverse iterators. */
static inline ptrdiff_t
vec_reverse_distance(struct vec_reverse_iterator first,
                     struct vec_reverse_iterator last)
{
    return (ptrdiff_t)last.offset - (ptrdiff_t)first.offset;
}

/** @brief Returns the distance between read-only reverse iterators. */
static inline ptrdiff_t
vec_reverse_distance_const(struct vec_const_reverse_iterator first,
                           struct vec_const_reverse_iterator last)
{
    return (ptrdiff_t)last.offset - (ptrdiff_t)first.offset;
}


/** @} */

/** @name Traversal */
/** @{ */

/**
 * @brief Iterates over mutable vector elements.
 *
 * Example:
 * @code
 * int *it;
 * vec_for_each(it, &v, int)
 *     printf("%d\\n", *it);
 * @endcode
 */

#define vec_for_each(pos, v, type)                           \
    for ((pos) = vec_begin_as((v), type);                   \
         (pos) != vec_end_as((v), type);                    \
         ++(pos))

/** @brief Iterates over read-only vector elements. */
#define vec_for_each_const(pos, v, type)                     \
    for ((pos) = vec_cbegin_as((v), type);                  \
         (pos) != vec_cend_as((v), type);                   \
         ++(pos))

/**
 * @brief Iterates over vector indexes in ascending order.
 */

#define vec_for_each_index(i, v) \
    for (size_t i = 0; i < (v)->size; ++i)

/** @brief Iterates over vector indexes in descending order. */
#define vec_for_each_index_reverse(i, v) \
    for (size_t i = (v)->size; i-- > 0; )

/** @} */

#endif /* VEC_H */
