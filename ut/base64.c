/* ut/base64.c
 * PPP over Any Transport -- Unit tests (BASE64)
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

#include "base64.h"
#include "memory.h"
#include "misc.h"	/* ARRAY_SIZE, pppoat_streq */
#include "ut/ut.h"

#include <string.h>	/* strlen */

/*
 * TODO:
 * 	- Binary test vectors with 0x00
 * 	- Encoding/deconding of a big random buffer
 */

struct ut_base64_test {
	const void *ubt_raw;
	size_t      ubt_raw_len;
	const char *ubt_base64;
};

static const struct ut_base64_test ut_base64_rfc4648_vector[] = {
	{
		.ubt_raw     = "",
		.ubt_raw_len = 0,
		.ubt_base64  = "",
	},
	{
		.ubt_raw     = "f",
		.ubt_raw_len = 1,
		.ubt_base64  = "Zg==",
	},
	{
		.ubt_raw     = "fo",
		.ubt_raw_len = 2,
		.ubt_base64  = "Zm8=",
	},
	{
		.ubt_raw     = "foo",
		.ubt_raw_len = 3,
		.ubt_base64  = "Zm9v",
	},
	{
		.ubt_raw     = "foob",
		.ubt_raw_len = 4,
		.ubt_base64  = "Zm9vYg==",
	},
	{
		.ubt_raw     = "fooba",
		.ubt_raw_len = 5,
		.ubt_base64  = "Zm9vYmE=",
	},
	{
		.ubt_raw     = "foobar",
		.ubt_raw_len = 6,
		.ubt_base64  = "Zm9vYmFy",
	},
};

static const struct ut_base64_test ut_base64_strings_vector[] = {
	{
		.ubt_raw     = "Some long message with only printable letters",
		.ubt_raw_len = 45,
		.ubt_base64  = "U29tZSBsb25nIG1lc3NhZ2Ugd2l0aC"
			       "Bvbmx5IHByaW50YWJsZSBsZXR0ZXJz",
	},
};

/* 15-byte array or zeros. */
static const char ut_base64_zero[] = {
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00,
};

static const struct ut_base64_test ut_base64_binary_vector[] = {
	{
		.ubt_raw     = ut_base64_zero,
		.ubt_raw_len = 15,
		.ubt_base64  = "AAAAAAAAAAAAAAAAAAAA",
	},
	{
		.ubt_raw     = ut_base64_zero,
		.ubt_raw_len = 14,
		.ubt_base64  = "AAAAAAAAAAAAAAAAAAA=",
	},
	{
		.ubt_raw     = ut_base64_zero,
		.ubt_raw_len = 13,
		.ubt_base64  = "AAAAAAAAAAAAAAAAAA==",
	},
};

static void ut_base64_run_vector(const struct ut_base64_test *vec, size_t nr)
{
	unsigned char *raw;
	char          *base64;
	size_t         len;
	size_t         i;
	int            rc;

	for (i = 0; i < nr; ++i) {
		base64 = NULL;
		rc = pppoat_base64_enc_new(vec[i].ubt_raw, vec[i].ubt_raw_len,
					   &base64);
		PPPOAT_ASSERT(rc == 0);
		PPPOAT_ASSERT(base64 != NULL);
		PPPOAT_ASSERT(pppoat_base64_is_valid(base64, strlen(base64)));
		PPPOAT_ASSERT(pppoat_streq(base64, vec[i].ubt_base64));

		raw = NULL;
		rc = pppoat_base64_dec_new(base64, strlen(base64), &raw, &len);
		PPPOAT_ASSERT(rc == 0);
		PPPOAT_ASSERT(raw != NULL);
		PPPOAT_ASSERT(len == vec[i].ubt_raw_len);
		PPPOAT_ASSERT(memcmp(raw, vec[i].ubt_raw, len) == 0);

		pppoat_free(raw);
		pppoat_free(base64);
	}
}

static void ut_base64_rfc4648(void)
{
	ut_base64_run_vector(ut_base64_rfc4648_vector,
			     ARRAY_SIZE(ut_base64_rfc4648_vector));
}

static void ut_base64_strings(void)
{
	ut_base64_run_vector(ut_base64_strings_vector,
			     ARRAY_SIZE(ut_base64_strings_vector));
}

static void ut_base64_binary(void)
{
	ut_base64_run_vector(ut_base64_binary_vector,
			     ARRAY_SIZE(ut_base64_binary_vector));
}

struct pppoat_ut_group pppoat_tests_base64 = {
	.ug_name = "base64",
	.ug_tests = {
		PPPOAT_UT_TEST("RFC4648", ut_base64_rfc4648),
		PPPOAT_UT_TEST("strings", ut_base64_strings),
		PPPOAT_UT_TEST("binary", ut_base64_binary),
		PPPOAT_UT_TEST_END,
	},
};
