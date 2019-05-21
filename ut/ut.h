/* ut.h
 * PPP over Any Transport -- Unit tests
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

#ifndef __PPPOAT_UT_H__
#define __PPPOAT_UT_H__

#include "list.h"

#include <stdbool.h>

/**
 * High level design.
 *
 * UT is a test framework. Each test is a small scenario along with a check
 * that the scenario leads to expected behaviour and/or result. All tests
 * are split into groups that reflect PPPoAT subsystems.
 *
 * If a test reveals unexpected behaviour or result UT must fail and provide
 * information where the fail occurs and what condition isn't met. Developer
 * can inspect the failure further.
 *
 * Motivation for this program is to catch some sort of bugs on early stages
 * of development. Also, some tests may be treated as example code.
 *
 * A group can have init/fini functions. If defined, they must be run before
 * and after the group's tests respectively. It is not required that all
 * tests from a group to be run and in the defined order. Tests may be reordered
 * across groups as well. Also, test or group can be run within some sort of
 * isolation (e.g. fork(2)). Therefore, developer must guarantee that tests
 * don't rely on other tests or order of execution.
 *
 * UT should support configurable features such as:
 *  - Running specified set of tests and/or groups;
 *  - Shuffling tests inside a group;
 *  - Running groups in parallel;
 */

/*
 * TODO
 * Requirements:
 *
 * - Run subset of tests
 * - Shuffle groups
 * - Shuffle tests within a group
 * - Shuffle tests beyond groups
 * - Run groups in parallel
 * - Run a test after fork(2)
 * - Measure performance
 *
 * Tests description options:
 *
 * 1. Arrays of tests/groups with hardcoded maximum number of elements.
 *    Overhead because of unused elements of the arrays.
 * 2. Interface pppoat_ut_{test,group}_add. Complicated code. Not convenient
 *    to disable a test, maintain list of tests.
 * 3. Mix of the above. Tests are described in arrays and groups are added via
 *    pppoat_ut_group_add().
 *
 * Running tests options:
 *
 * 1. List of objects:
 *    a. Object is a group. Groups must not be `const`. Can create new group
 *       objects for subset of tests or shuffled tests. In worst case, need to
 *       allocate number of groups equal to number of tests.
 *    b. Object is a test. Tests must not be `const`. Every test contains link
 *       and pointer to respective group. Initialised on pppoat_ut_run().
 *    c. Object is a separated structure, which points to test and its group.
 *       Tests and groups may be `const`. Need to allocate number of objects.
 *       Memory overhead depends on number of running tests.
 */

enum {
	PPPOAT_UT_TESTS_NR_MAX  = 32,
};

#define PPPOAT_UT_TEST(name, func) { .ut_name = name, .ut_func = func, }
#define PPPOAT_UT_TEST_DISABLED(name, func) \
		{ .ut_name = name, .ut_func = func, .ut_disabled = true, }
#define PPPOAT_UT_TEST_END { .ut_name = NULL, .ut_func = NULL, }


struct pppoat_ut_test {
	const char *ut_name;
	void      (*ut_func)(void);
	bool        ut_disabled;
};

struct pppoat_ut_group {
	const char              *ug_name;
	void                   (*ug_init)(void);
	void                   (*ug_fini)(void);
	struct pppoat_ut_test    ug_tests[PPPOAT_UT_TESTS_NR_MAX];
	bool                     ug_disabled;
	struct pppoat_list_link  ug_link;
	uint32_t                 ug_magic;
};

struct pppoat_ut {
	/* List of groups. */
	struct pppoat_list u_list;
};

void pppoat_ut_init(struct pppoat_ut *ut);
void pppoat_ut_fini(struct pppoat_ut *ut);

void pppoat_ut_group_add(struct pppoat_ut *ut, struct pppoat_ut_group *group);

void pppoat_ut_run(struct pppoat_ut *ut);

#endif /* __PPPOAT_UT_H__ */
