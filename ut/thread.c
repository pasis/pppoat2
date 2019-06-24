/* ut/thread.c
 * PPP over Any Transport -- Unit tests (Threads)
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

#include "memory.h"
#include "thread.h"
#include "ut/ut.h"

#include <string.h>

enum {
	UT_THREAD_NR = 5,
};

void ut_thread_start_join_cb(struct pppoat_thread *thread)
{
	int *counter_ptr = thread->t_userdata;

	++(*counter_ptr);
}

void ut_thread_start_join(void)
{
	struct pppoat_thread *threads;
	int                  *arr;
	size_t                arr_size;
	int                   reduce = 0;
	int                   i;
	int                   rc;

	threads = pppoat_alloc(sizeof *threads * UT_THREAD_NR);
	PPPOAT_ASSERT(threads != NULL);
	arr_size = sizeof(int) * UT_THREAD_NR;
	arr = pppoat_alloc(arr_size);
	PPPOAT_ASSERT(arr != NULL);
	memset(arr, 0, arr_size);

	for (i = 0; i < UT_THREAD_NR; ++i) {
		rc = pppoat_thread_init(&threads[i], &ut_thread_start_join_cb);
		PPPOAT_ASSERT(rc == 0);
		threads[i].t_userdata = &arr[i];
		rc = pppoat_thread_start(&threads[i]);
		PPPOAT_ASSERT(rc == 0);
	}
	for (i = 0; i < UT_THREAD_NR; ++i) {
		rc = pppoat_thread_join(&threads[i]);
		PPPOAT_ASSERT(rc == 0);
		reduce += arr[i];
		pppoat_thread_fini(&threads[i]);
	}
	PPPOAT_ASSERT(reduce == UT_THREAD_NR);

	pppoat_free(arr);
	pppoat_free(threads);
}

struct pppoat_ut_group pppoat_tests_thread = {
	.ug_name = "thread",
	.ug_tests = {
		PPPOAT_UT_TEST("start-join", ut_thread_start_join),
		PPPOAT_UT_TEST_END,
	},
};
