/* ut/conf.c
 * PPP over Any Transport -- Unit tests (Linked lists)
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

#include "conf.h"
#include "memory.h"
#include "ut/ut.h"

#include <string.h>	/* strcmp */

static const char *ut_conf_simple_key = "simple_key";
static const char *ut_conf_simple_val = "simple_val";

static void ut_conf_simple(void)
{
	struct pppoat_conf_record *r;
	struct pppoat_conf        *conf;
	char                      *str;
	int                        rc;

	conf = pppoat_alloc(sizeof *conf);
	PPPOAT_ASSERT(conf != NULL);

	rc = pppoat_conf_init(conf);
	PPPOAT_ASSERT(rc == 0);

	rc = pppoat_conf_store(conf, ut_conf_simple_key, ut_conf_simple_val);
	PPPOAT_ASSERT(rc == 0);

	r = pppoat_conf_lookup(conf, ut_conf_simple_key);
	PPPOAT_ASSERT(r != NULL);
	PPPOAT_ASSERT(strcmp(r->cr_val, ut_conf_simple_val) == 0);
	pppoat_conf_record_put(r);

	rc = pppoat_conf_find_string_alloc(conf, ut_conf_simple_key, &str);
	PPPOAT_ASSERT(rc == 0);
	PPPOAT_ASSERT(strcmp(str, ut_conf_simple_val) == 0);
	pppoat_free(str);

	pppoat_conf_drop(conf, ut_conf_simple_key);
	r = pppoat_conf_lookup(conf, ut_conf_simple_key);
	PPPOAT_ASSERT(r == NULL);

	pppoat_conf_fini(conf);
	pppoat_free(conf);
}

struct pppoat_ut_group pppoat_tests_conf = {
	.ug_name = "conf",
	.ug_tests = {
		PPPOAT_UT_TEST("simple", ut_conf_simple),
		PPPOAT_UT_TEST_END,
	},
};
