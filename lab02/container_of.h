/* SPDX-License-Identifier: GPL-2.0 */
/**
 * @file container_of.h
 * @brief Provides type-aware container and member offset macros.
 */
#ifndef _CONTAINER_OF_H
#define _CONTAINER_OF_H

#include <stddef.h>

/** @brief Checks whether two expressions have compatible types. */
#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

/** @brief Computes a member offset using the compiler builtin. */
#undef offsetof
#define offsetof(TYPE, MEMBER)	__builtin_offsetof(TYPE, MEMBER)

/** @brief Obtains the type of a member of a structure type. */
#define typeof_member(T, m)   typeof(((T*)0)->m)

/**
 * @brief Converts a pointer to a structure member to its containing object.
 * @param ptr A pointer to the member.
 * @param type The type of the containing structure.
 * @param member The member name within `type`.
 *
 * The macro performs a compile-time type compatibility check before applying
 * the member offset.
 */
#define container_of(ptr, type, member) ({                         \
    _Static_assert(__same_type(*(ptr), typeof_member(type, member)) || \
                   __same_type(*(ptr), void),                      \
                   "pointer type mismatch in container_of()");     \
    (type *)((void *)(ptr) - offsetof(type, member)); })


/**
 * @brief Converts a member pointer while preserving its const qualification.
 * @param ptr A pointer to the member.
 * @param type The type of the containing structure.
 * @param member The member name within `type`.
 */
#define container_of_const(ptr, type, member)				\
	_Generic(ptr,							\
		const typeof(*(ptr)) *: ((const type *)container_of(ptr, type, member)),\
		default: ((type *)container_of(ptr, type, member))	\
	)

#endif /* _CONTAINER_OF_H */
