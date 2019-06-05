/* ut/sem.c
 * PPP over Any Transport -- Unit tests (Semaphores)
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

#include "sem.h"
#include "ut/ut.h"

enum {
	UT_SEM_ITER_NR = 5,
};

static void ut_sem_trywait(void)
{
	struct pppoat_semaphore sem;
	int                     i;
	bool                    result;

	pppoat_semaphore_init(&sem, 0);
	for (i = 0; i < UT_SEM_ITER_NR; ++i) {
		result = pppoat_semaphore_trywait(&sem);
		PPPOAT_ASSERT(!result);
	}
	pppoat_semaphore_post(&sem);
	result = pppoat_semaphore_trywait(&sem);
	PPPOAT_ASSERT(result);
	for (i = 0; i < UT_SEM_ITER_NR; ++i) {
		result = pppoat_semaphore_trywait(&sem);
		PPPOAT_ASSERT(!result);
	}
	pppoat_semaphore_fini(&sem);
}

struct pppoat_ut_group pppoat_tests_sem = {
	.ug_name = "semaphores",
	.ug_tests = {
		PPPOAT_UT_TEST("trywait", ut_sem_trywait),
		PPPOAT_UT_TEST_END,
	},
};
