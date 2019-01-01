/* ut.c
 * PPP over Any Transport -- Unit tests
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

#include "ut/ut.h"

#include <stdio.h>

static const struct pppoat_list_descr ut_list_descr =
	PPPOAT_LIST_DESCR("UT list", struct pppoat_ut_group,
			  ug_link, ug_magic, 0xca11ab1e);

void pppoat_ut_init(struct pppoat_ut *ut)
{
	pppoat_list_init(&ut->u_list, &ut_list_descr);
}

void pppoat_ut_fini(struct pppoat_ut *ut)
{
	/* Flush the list. */
	while (!pppoat_list_is_empty(&ut->u_list))
		(void)pppoat_list_pop(&ut->u_list);
	pppoat_list_fini(&ut->u_list);
}

void pppoat_ut_group_add(struct pppoat_ut *ut, struct pppoat_ut_group *group)
{
	pppoat_list_insert_tail(&ut->u_list, group);
}

bool ut_test_is_last(struct pppoat_ut_test *test)
{
	return test->ut_name == NULL && test->ut_func == NULL;
}

void pppoat_ut_run(struct pppoat_ut *ut)
{
	struct pppoat_ut_group *group;
	struct pppoat_ut_test  *test;
	int                     nr = 0;
	int                     i;

	for (group = pppoat_list_head(&ut->u_list); group != NULL;
	     group = pppoat_list_next(&ut->u_list, group)) {
		printf("%s\n", group->ug_name);
		if (group->ug_init != NULL)
			group->ug_init();
		for (i = 0; !ut_test_is_last(&group->ug_tests[i]); ++i) {
			test = &group->ug_tests[i];
			printf("  %s... ", test->ut_name);
			(void)fflush(stdout);
			test->ut_func();
			printf("OK\n");
			++nr;
		}
		if (group->ug_fini != NULL)
			group->ug_fini();
		printf("\n");
	}

	printf("Ran %d tests in %d groups.\n", nr,
	       pppoat_list_count(&ut->u_list));
}
