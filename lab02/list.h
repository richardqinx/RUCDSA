/* SPDX-License-Identifier: GPL-2.0 */

/**
 * @file list.h
 * @brief Intrusive circular doubly linked list primitives.
 *
 * A list node is embedded in the structure that owns it. The list head is
 * represented by a sentinel `struct list_head` whose links point to itself
 * when the list is empty.
 */
#ifndef LIST_H
#define LIST_H

#include <stddef.h>
#include <stdbool.h>
#include <assert.h>

#include "container_of.h"

/**
 * @brief A node in an intrusive circular doubly linked list.
 */
struct list_head {
  struct list_head *next;
  struct list_head *prev;
};

/** @brief Poison value written to a deleted node's `next` link. */
#define LIST_POISON1  ((void *) 0x100)

/** @brief Poison value written to a deleted node's `prev` link. */
#define LIST_POISON2  ((void *) 0x122)

/**
 * @brief Circular doubly linked list implementation.
 *
 * Some internal functions are useful when manipulating whole lists because
 * the neighboring nodes are already known.
 */

/** @brief Initializes a list head initializer. */
#define LIST_HEAD_INIT(name) { &(name), &(name) }

/**
 * @brief Declares and initializes an empty list head.
 * @param name The list head variable name.
 */
#define LIST_HEAD(name) \
    struct list_head name = LIST_HEAD_INIT(name)

/**
 * @brief Initializes a list node or list head.
 *
 * The node is made self-linked. When used as a list head, this represents
 * an empty list.
 *
 * @param list The node or list head to initialize.
 * @post `list->next == list`.
 * @post `list->prev == list`.
 */
static inline void INIT_LIST_HEAD(struct list_head *list)
{
    list->next = list;
    list->prev = list;
}

/**
 * @internal
 * @brief Links a node between two known adjacent nodes.
 */
static inline void __list_add(struct list_head *node,
                              struct list_head *prev,
                              struct list_head *next)
{
    next->prev = node;
    node->next = next;
    node->prev = prev;
    prev->next = node;
}

/**
 * @brief Inserts a node immediately after a list node.
 *
 * This operation is suitable for stack-like insertion.
 *
 * @param node The node to insert.
 * @param head The node after which `node` is inserted.
 * @pre `node` is not currently linked into a list.
 */
static inline void list_add(struct list_head *node,
                            struct list_head *head)
{
    __list_add(node, head, head->next);
}

/**
 * @brief Inserts a node immediately before a list node.
 *
 * This operation is suitable for queue-like insertion when `head` is the
 * list sentinel.
 *
 * @param node The node to insert.
 * @param head The node before which `node` is inserted.
 * @pre `node` is not currently linked into a list.
 */
static inline void list_add_tail(struct list_head *node,
                                 struct list_head *head)
{
    __list_add(node, head->prev, head);
}

/**
 * @internal
 * @brief Unlinks the range between two known adjacent nodes.
 */
static inline void __list_del(struct list_head *prev,
                              struct list_head *next)
{
    next->prev = prev;
    prev->next = next;
}

/**
 * @internal
 * @brief Unlinks a node using its stored neighboring links.
 */
static inline void __list_del_entry(struct list_head *entry)
{
	__list_del(entry->prev, entry->next);
}

/**
 * @brief Removes a node from its list.
 *
 * After this operation, the node's links contain poison values and must not
 * be used for list traversal.
 *
 * @param entry The node to remove.
 * @pre `entry` belongs to a valid list.
 */
static inline void list_del(struct list_head *entry)
{
	__list_del_entry(entry);
	entry->next = LIST_POISON1;
	entry->prev = LIST_POISON2;
}

/**
 * @brief Replaces one list node with another.
 *
 * The replacement node takes the position of the old node.
 *
 * @param old The node to replace.
 * @param new The replacement node.
 * @pre `old` belongs to a valid list.
 * @pre `new` is not currently linked into a list.
 */
static inline void list_replace(struct list_head *old,
				struct list_head *new)
{
	new->next = old->next;
	new->next->prev = new;
	new->prev = old->prev;
	new->prev->next = new;
}

/**
 * @brief Replaces one node and reinitializes the old node.
 *
 * @param old The node to replace.
 * @param new The replacement node.
 * @pre `old` belongs to a valid list.
 * @pre `new` is not currently linked into a list.
 * @post `old` is a self-linked standalone node.
 */
static inline void list_replace_init(struct list_head *old,
				     struct list_head *new)
{
	list_replace(old, new);
	INIT_LIST_HEAD(old);
}

/**
 * @brief Removes a node from its list and inserts it after `head`.
 *
 * @param list The node to move.
 * @param head The node that precedes the moved node.
 * @pre `list` belongs to a valid list.
 */
static inline void list_move(struct list_head *list, struct list_head *head)
{
	__list_del_entry(list);
	list_add(list, head);
}

/**
 * @brief Removes a node from its list and inserts it before `head`.
 *
 * @param list The node to move.
 * @param head The node that follows the moved node.
 * @pre `list` belongs to a valid list.
 */
static inline void list_move_tail(struct list_head *list,
				  struct list_head *head)
{
	__list_del_entry(list);
	list_add_tail(list, head);
}

/**
 * @brief Tests whether a node is the first entry in a list.
 *
 * @param list The node to test.
 * @param head The list head.
 * @return Nonzero if `list` is the first entry; zero otherwise.
 */
static inline int list_is_first(const struct list_head *list, const struct list_head *head)
{
	return list->prev == head;
}

/**
 * @brief Tests whether a node is the last entry in a list.
 *
 * @param list The node to test.
 * @param head The list head.
 * @return Nonzero if `list` is the last entry; zero otherwise.
 */
static inline int list_is_last(const struct list_head *list, const struct list_head *head)
{
	return list->next == head;
}

/**
 * @brief Tests whether a node is the list head.
 *
 * @param list The node to test.
 * @param head The list head.
 * @return Nonzero if `list == head`; zero otherwise.
 */
static inline int list_is_head(const struct list_head *list, const struct list_head *head)
{
	return list == head;
}

/**
 * @brief Tests whether a list is empty.
 *
 * @param head The list head.
 * @return Nonzero if the list contains no entries; zero otherwise.
 */
static inline int list_empty(const struct list_head *head)
{
	return head->next == head;
}

/**
 * @brief Returns the containing structure for a list node.
 *
 * @param ptr A pointer to the embedded `struct list_head`.
 * @param type The containing structure type.
 * @param member The embedded list member.
 * @return A pointer to the containing structure.
 */
#define list_entry(ptr, type, member) \
	container_of(ptr, type, member)

/**
 * @brief Returns the first containing structure in a list.
 *
 * @param ptr The list head.
 * @param type The containing structure type.
 * @param member The embedded list member.
 * @pre The list is not empty.
 */
#define list_first_entry(ptr, type, member) \
	list_entry((ptr)->next, type, member)

/**
 * @brief Returns the last containing structure in a list.
 *
 * @param ptr The list head.
 * @param type The containing structure type.
 * @param member The embedded list member.
 * @pre The list is not empty.
 */
#define list_last_entry(ptr, type, member) \
	list_entry((ptr)->prev, type, member)

/**
 * @brief Returns the containing structure following the current node.
 *
 * @param pos The typed cursor.
 * @param member The embedded list member.
 * @return The next containing structure.
 */
#define list_next_entry(pos, member) \
	list_entry((pos)->member.next, typeof(*(pos)), member)

/**
 * @brief Returns the containing structure preceding the current node.
 *
 * @param pos The typed cursor.
 * @param member The embedded list member.
 * @return The previous containing structure.
 */
#define list_prev_entry(pos, member) \
	list_entry((pos)->member.prev, typeof(*(pos)), member)

/**
 * @brief Tests whether a typed cursor refers to the list head.
 *
 * @param pos The typed cursor.
 * @param head The list head.
 * @param member The embedded list member.
 * @return Nonzero if `pos` refers to `head`; zero otherwise.
 */
#define list_entry_is_head(pos, head, member)				\
	(&pos->member == (head))

/**
 * @brief Iterates over containing structures in forward order.
 *
 * Example:
 * @code
 * struct item *pos;
 * list_for_each_entry(pos, &head, link) {
 *     use_item(pos);
 * }
 * @endcode
 *
 * @param pos The typed loop cursor.
 * @param head The list head.
 * @param member The embedded list member.
 */
#define list_for_each_entry(pos, head, member)				\
	for (pos = list_first_entry(head, typeof(*pos), member);	\
	     !list_entry_is_head(pos, head, member);			\
	     pos = list_next_entry(pos, member))

/**
 * @brief Iterates over containing structures in reverse order.
 *
 * @param pos The typed loop cursor.
 * @param head The list head.
 * @param member The embedded list member.
 */
#define list_for_each_entry_reverse(pos, head, member)			\
	for (pos = list_last_entry(head, typeof(*pos), member);		\
	     !list_entry_is_head(pos, head, member); 			\
	     pos = list_prev_entry(pos, member))

/**
 * @brief Iterates over containing structures while allowing removal.
 *
 * The additional cursor stores the next entry before the loop body runs, so
 * the current entry may be removed safely from the list.
 *
 * @param pos The typed loop cursor.
 * @param n The temporary typed cursor.
 * @param head The list head.
 * @param member The embedded list member.
 */
#define list_for_each_entry_safe(pos, n, head, member)			\
	for (pos = list_first_entry(head, typeof(*pos), member),	\
		n = list_next_entry(pos, member);			\
	     !list_entry_is_head(pos, head, member);			\
	     pos = n, n = list_next_entry(n, member))

#endif /* LIST_H */
