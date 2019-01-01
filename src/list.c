/* list.c
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

#include "trace.h"

#include "list.h"

static bool list_invariant(struct pppoat_list *list)
{
	const struct pppoat_list_descr *descr = list->l_descr;
	struct pppoat_list_link        *head  = &list->l_head;

	return descr != NULL &&
	       list->l_magic == descr->ld_magic &&
	       ((head->ll_next == head && head->ll_prev == head) ||
		(head->ll_next != head && head->ll_prev != head));
}

void pppoat_list_init(struct pppoat_list             *list,
		      const struct pppoat_list_descr *descr)
{
	PPPOAT_ASSERT(descr->ld_name != NULL);

	list->l_descr = descr;
	list->l_head.ll_next = &list->l_head;
	list->l_head.ll_prev = &list->l_head;
	list->l_magic = descr->ld_magic;

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(pppoat_list_is_empty(list));
}

void pppoat_list_fini(struct pppoat_list *list)
{
	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(pppoat_list_is_empty(list));

	/* TODO Poisoning in debug mode */
}

static uint32_t *list_obj_magic(struct pppoat_list *list, void *obj)
{
	return (uint32_t *)((char *)obj + list->l_descr->ld_magic_off);
}

static void list_obj_magic_set(struct pppoat_list *list, void *obj)
{
	*list_obj_magic(list, obj) = list->l_descr->ld_magic;
}

static bool list_obj_magic_is_correct(struct pppoat_list *list, void *obj)
{
	uint32_t magic = *list_obj_magic(list, obj);

	return magic == list->l_descr->ld_magic;
}

static struct pppoat_list_link *list_obj_link(struct pppoat_list *list,
					      void               *obj)
{
	return (struct pppoat_list_link *)((char *)obj +
					   list->l_descr->ld_link_off);
}

static void *list_link2obj(struct pppoat_list      *list,
			   struct pppoat_list_link *link)
{
	void *obj = (char *)link - list->l_descr->ld_link_off;

	PPPOAT_ASSERT(list_obj_magic_is_correct(list, obj));
	return obj;
}

static void list_insert_before_link(struct pppoat_list      *list,
				    void                    *obj,
				    struct pppoat_list_link *before)
{
	struct pppoat_list_link *link = list_obj_link(list, obj);

	PPPOAT_ASSERT(list_invariant(list));

	link->ll_next = before;
	link->ll_prev = before->ll_prev;
	link->ll_prev->ll_next = link;
	before->ll_prev = link;

	list_obj_magic_set(list, obj);
}

static void list_insert_after_link(struct pppoat_list      *list,
				   void                    *obj,
				   struct pppoat_list_link *after)
{
	struct pppoat_list_link *link = list_obj_link(list, obj);

	PPPOAT_ASSERT(list_invariant(list));

	link->ll_next = after->ll_next;
	link->ll_prev = after;
	link->ll_next->ll_prev = link;
	after->ll_next = link;

	list_obj_magic_set(list, obj);
}

void pppoat_list_insert(struct pppoat_list *list, void *obj)
{
	pppoat_list_insert_head(list, obj);
}

void pppoat_list_insert_head(struct pppoat_list *list, void *obj)
{
	list_insert_after_link(list, obj, &list->l_head);
}

void pppoat_list_insert_tail(struct pppoat_list *list, void *obj)
{
	list_insert_before_link(list, obj, &list->l_head);
}

void pppoat_list_insert_before(struct pppoat_list *list,
			       void               *obj,
			       void               *before)
{
	struct pppoat_list_link *link_before = list_obj_link(list, before);

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(list_obj_magic_is_correct(list, before));

	list_insert_before_link(list, obj, link_before);
}

void pppoat_list_insert_after(struct pppoat_list *list,
			      void               *obj,
			      void               *after)
{
	struct pppoat_list_link *link_after = list_obj_link(list, after);

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(list_obj_magic_is_correct(list, after));

	list_insert_after_link(list, obj, link_after);
}

void pppoat_list_del(struct pppoat_list *list, void *obj)
{
	struct pppoat_list_link *link = list_obj_link(list, obj);

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(list_obj_magic_is_correct(list, obj));

	link->ll_prev->ll_next = link->ll_next;
	link->ll_next->ll_prev = link->ll_prev;

	link->ll_next = link;
	link->ll_prev = link;
}

void pppoat_list_push(struct pppoat_list *list, void *obj)
{
	pppoat_list_insert_head(list, obj);
}

void *pppoat_list_pop(struct pppoat_list *list)
{
	void *obj = pppoat_list_head(list);

	pppoat_list_del(list, obj);
	return obj;
}

void pppoat_list_enqueue(struct pppoat_list *list, void *obj)
{
	pppoat_list_insert_tail(list, obj);
}

void *pppoat_list_dequeue(struct pppoat_list *list)
{
	void *obj = pppoat_list_head(list);

	pppoat_list_del(list, obj);
	return obj;
}

void *pppoat_list_head(struct pppoat_list *list)
{
	return pppoat_list_is_empty(list) ? NULL :
	       list_link2obj(list, list->l_head.ll_next);
}

void *pppoat_list_tail(struct pppoat_list *list)
{
	return pppoat_list_is_empty(list) ? NULL :
	       list_link2obj(list, list->l_head.ll_prev);
}

void *pppoat_list_next(struct pppoat_list *list, void *obj)
{
	struct pppoat_list_link *link = list_obj_link(list, obj);

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(list_obj_magic_is_correct(list, obj));
	link = link->ll_next;

	return link == &list->l_head ? NULL : list_link2obj(list, link);
}

void *pppoat_list_prev(struct pppoat_list *list, void *obj)
{
	struct pppoat_list_link *link = list_obj_link(list, obj);

	PPPOAT_ASSERT(list_invariant(list));
	PPPOAT_ASSERT(list_obj_magic_is_correct(list, obj));
	link = link->ll_prev;

	return link == &list->l_head ? NULL : list_link2obj(list, link);
}

bool pppoat_list_is_empty(struct pppoat_list *list)
{
	struct pppoat_list_link *head = &list->l_head;
	PPPOAT_ASSERT(list_invariant(list));

	return head->ll_next == head && head->ll_prev == head;
}

int pppoat_list_count(struct pppoat_list *list)
{
	struct pppoat_list_link *link = &list->l_head;
	int count;

	PPPOAT_ASSERT(list_invariant(list));
	for (count = 0; link->ll_next != &list->l_head; ++count)
		link = link->ll_next;

	return count;
}
