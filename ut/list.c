/* ut/list.c
 * PPP over Any Transport -- Unit tests (Linked lists)
 *
 * Copyright (C) 2012-2019 Dmitry Podgorny <pasis.ua@gmail.com>
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
#include "memory.h"
#include "ut/ut.h"

enum {
	UT_LIST_HEADER = 0x12345678,
	UT_LIST_MIDDLE = 0x7F6F5F4F,
	UT_LIST_FOOTER = 0x1E2E3E4E,
	UT_LIST_MAGIC  = 0xFEEDBEE5,

	UT_LIST_NR     = 5,
};

struct ut_list_st {
	int                     uls_header;
	struct pppoat_list_link uls_link;
	int                     uls_middle;
	uint32_t                uls_magic;
	int                     uls_footer;
};

static struct pppoat_list_descr ut_list_descr =
	PPPOAT_LIST_DESCR("list UT", struct ut_list_st, uls_link,
			  uls_magic, UT_LIST_MAGIC);

/*
 * Allocates new object and initialises it according to its `index` number
 * in a list. Further, initialised magics will be checked to catch corruptions.
 * This function always returns new object or causes a crash on ENOMEM error.
 */
static struct ut_list_st *ut_list_obj_new(int index)
{
	struct ut_list_st *obj = pppoat_alloc(sizeof(*obj));

	PPPOAT_ASSERT(obj != NULL);
	obj->uls_header = UT_LIST_HEADER + index;
	obj->uls_middle = UT_LIST_MIDDLE + index;
	obj->uls_footer = UT_LIST_FOOTER + index;

	return obj;
}

/*
 * Destroys object and checks whether it is corrupted or not.
 */
static void ut_list_obj_destroy(struct ut_list_st *obj, int index)
{
	PPPOAT_ASSERT(obj->uls_header == UT_LIST_HEADER + index);
	PPPOAT_ASSERT(obj->uls_middle == UT_LIST_MIDDLE + index);
	PPPOAT_ASSERT(obj->uls_footer == UT_LIST_FOOTER + index);
	pppoat_free(obj);
}

static void ut_list_insert(void)
{
	struct pppoat_list  list;
	struct ut_list_st  *obj;
	struct ut_list_st  *obj2;
	int                 nr;
	int                 i;

	for (nr = 0; nr < UT_LIST_NR; ++nr) {
		pppoat_list_init(&list, &ut_list_descr);
		for (i = 0; i < nr; ++i) {
			obj = ut_list_obj_new(i);
			pppoat_list_insert_tail(&list, obj);
		}
		PPPOAT_ASSERT(pppoat_list_count(&list) == nr);

		for (obj = pppoat_list_head(&list), i = 0; i < nr; ++i) {
			PPPOAT_ASSERT(obj != NULL);
			obj2 = pppoat_list_next(&list, obj);
			pppoat_list_del(&list, obj);
			ut_list_obj_destroy(obj, i);
			obj = obj2;
		}
		PPPOAT_ASSERT(obj == NULL);
		pppoat_list_fini(&list);
	}
}

struct pppoat_ut_group pppoat_tests_list = {
	.ug_name = "list",
	.ug_tests = {
		PPPOAT_UT_TEST("insert", ut_list_insert),
		PPPOAT_UT_TEST_END,
	},
};
