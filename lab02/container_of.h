/* SPDX-License-Identifier: GPL-2.0 */
#ifndef _CONTAINER_OF_H
#define _CONTAINER_OF_H

#include <stddef.h>

#define __same_type(a, b) __builtin_types_compatible_p(typeof(a), typeof(b))

#undef offsetof
#define offsetof(TYPE, MEMBER)	__builtin_offsetof(TYPE, MEMBER)

#define typeof_member(T, m)   typeof(((T*)0)->m)


#define container_of(ptr, type, member) ({                         \
    _Static_assert(__same_type(*(ptr), typeof_member(type, member)) || \
                   __same_type(*(ptr), void),                      \
                   "pointer type mismatch in container_of()");     \
    (type *)((void *)(ptr) - offsetof(type, member)); })


#define container_of_const(ptr, type, member)				\
	_Generic(ptr,							\
		const typeof(*(ptr)) *: ((const type *)container_of(ptr, type, member)),\
		default: ((type *)container_of(ptr, type, member))	\
	)

#endif /* _CONTAINER_OF_H */
