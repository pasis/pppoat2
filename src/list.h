/* list.h
 * PPP over Any Transport -- Linked lists
 *
 * Copyright (C) 2012-2018 Dmitry Podgorny <pasis.ua@gmail.com>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef __PPPOAT_LIST_H__
#define __PPPOAT_LIST_H__

#include <stdbool.h>	/* bool */
#include <stddef.h>	/* offsetof */
#include <stdint.h>	/* uint32_t */

/**
 * High level design.
 *
 * List should provide generic interface and implement doubly-linked lists. It
 * should be efficient for wide range of use-cases.
 *
 * List interface must not fail. Including operations with an existent element.
 */

/**
 * Detailed level design.
 *
 * There is no list-specific containers. Metadata required for linking elements
 * into a list is embedded into objects. The embedded field is pppoat_list_link.
 * Therefore, an object itself becomes a list element. With this approach list
 * subsystem doesn't allocate memory for metadata and doesn't fail as result.
 *
 * Embedding link structure into an abject provides additional advantages:
 *  - User may control in what memory the metadata is stored;
 *  - More convenient list interface as user's object becomes an argument
 *    for the interface.
 *
 * List is looped. It is done for simplification of implementation.
 *
 * Additionally to the link field, user's object contains a magic field. Its
 * purpose is to catch list/memory corruptions and handle incorrect pointers
 * provided by user. Every element in a list is assigned with a particular
 * magic value. The magic value is an unique 32-bit number provided by user.
 * Preferably, in human-readable format (e.g. 0xDEADBEAF).
 *
 * pppoat_list_descr describes a list type. It contains information about link
 * and magic fields. There can be multiple instances of the same list type.
 *
 * List interface is not thread-safe and user must serialise access to a list.
 * For example, by protecting operation (or set of operations) with a mutex.
 * Concurrent access to a list can lead to the list corruption.
 */

struct pppoat_list_descr {
	const char    *ld_name;
	unsigned long  ld_link_off;
	unsigned long  ld_magic_off;
	uint32_t       ld_magic;
};

struct pppoat_list_link {
	struct pppoat_list_link *ll_prev;
	struct pppoat_list_link *ll_next;
};

struct pppoat_list {
	const struct pppoat_list_descr *l_descr;
	struct pppoat_list_link         l_head;
	uint32_t                        l_magic;
};

void pppoat_list_init(struct pppoat_list             *list,
		      const struct pppoat_list_descr *descr);
void pppoat_list_fini(struct pppoat_list *list);

void pppoat_list_insert(struct pppoat_list *list, void *obj);
void pppoat_list_insert_head(struct pppoat_list *list, void *obj);
void pppoat_list_insert_tail(struct pppoat_list *list, void *obj);
void pppoat_list_insert_before(struct pppoat_list *list,
			       void               *obj,
			       void               *before);
void pppoat_list_insert_after(struct pppoat_list *list,
			      void               *obj,
			      void               *after);

void pppoat_list_del(struct pppoat_list *list, void *obj);

void pppoat_list_push(struct pppoat_list *list, void *obj);
void *pppoat_list_pop(struct pppoat_list *list);

void pppoat_list_enqueue(struct pppoat_list *list, void *obj);
void *pppoat_list_dequeue(struct pppoat_list *list);

void *pppoat_list_head(struct pppoat_list *list);
void *pppoat_list_tail(struct pppoat_list *list);
void *pppoat_list_next(struct pppoat_list *list, void *obj);
void *pppoat_list_prev(struct pppoat_list *list, void *obj);

bool pppoat_list_is_empty(struct pppoat_list *list);

int pppoat_list_count(struct pppoat_list *list);

#define PPPOAT_LIST_DESCR(name, type, link_field, magic_field, magic) \
{                                                                     \
	.ld_name      = name,                                         \
	.ld_link_off  = offsetof(type, link_field),                   \
	.ld_magic_off = offsetof(type, magic_field),                  \
	.ld_magic     = magic,                                        \
}

#endif /* __PPPOAT_LIST_H__ */
