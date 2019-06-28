/* ut/trace.c
 * PPP over Any Transport -- Unit tests (asserts)
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

/*
 * Current UT checks that asserts don't cause "unused" warnings with NDEBUG.
 * This test is used for compile-time checks and doesn't do important job in
 * run-time.
 */

#ifndef NDEBUG
#define NDEBUG
#define UT_TRACE_NDEBUG_DEFINED 1
#endif

#include "trace.h"

#include "ut/ut.h"

static int ut_trace_unused1(void)
{
	return 0;
}

static int ut_trace_unused2(void)
{
	return 0;
}

static int ut_trace_unused3(void)
{
	return 0;
}

static int ut_trace_errno(void)
{
	return -1;
}

static void ut_trace_compile(void)
{
	int var = 0;
	int rc;

	PPPOAT_ASSERT(var == 0);
	PPPOAT_ASSERT_INFO(var == 0, "%d", ut_trace_unused1());
	PPPOAT_ASSERT(ut_trace_unused2() == 0);
	if (false)
		pppoat_debug("ut", "%d", ut_trace_unused3());
	if (false) {
		rc = P_ERR(ut_trace_errno());
		(void)rc;
	}
}

struct pppoat_ut_group pppoat_tests_trace = {
	.ug_name = "trace",
	.ug_tests = {
		PPPOAT_UT_TEST("compile", ut_trace_compile),
		PPPOAT_UT_TEST_END,
	},
};

#ifdef UT_TRACE_NDEBUG_DEFINED
#undef UT_TRACE_NDEBUG_DEFINED
#undef NDEBUG
#endif
