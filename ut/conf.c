/* ut/conf.c
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

#include "conf.h"
#include "memory.h"
#include "misc.h"	/* pppoat_streq */
#include "ut/ut.h"

#include <string.h>

static const char *ut_conf_simple_key = "simple_key";
static const char *ut_conf_simple_val = "simple_val";
static const char *ut_conf_file_path = "ut/pppoat.conf";

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
	PPPOAT_ASSERT(pppoat_streq(r->cr_val, ut_conf_simple_val));
	pppoat_conf_record_put(r);

	rc = pppoat_conf_find_string_alloc(conf, ut_conf_simple_key, &str);
	PPPOAT_ASSERT(rc == 0);
	PPPOAT_ASSERT(pppoat_streq(str, ut_conf_simple_val));
	pppoat_free(str);

	pppoat_conf_drop(conf, ut_conf_simple_key);
	r = pppoat_conf_lookup(conf, ut_conf_simple_key);
	PPPOAT_ASSERT(r == NULL);

	pppoat_conf_fini(conf);
	pppoat_free(conf);
}

static void ut_conf_file(void)
{
	struct pppoat_conf        *conf;
	char                      *opt1;
	char                      *opt2;
	char                      *opt3;
	long                       opt4;
	bool                       opt5;
	int                        rc;

	conf = pppoat_alloc(sizeof *conf);
	PPPOAT_ASSERT(conf != NULL);

	rc = pppoat_conf_init(conf);
	PPPOAT_ASSERT(rc == 0);

	rc = pppoat_conf_read_file(conf, ut_conf_file_path);
	PPPOAT_ASSERT(rc == 0);

	rc = pppoat_conf_find_string_alloc(conf, "interface", &opt1)
	  ?: pppoat_conf_find_string_alloc(conf, "transport", &opt2)
	  ?: pppoat_conf_find_string_alloc(conf, "pppd.ip", &opt3)
	  ?: pppoat_conf_find_long(conf, "udp.port", &opt4);
	pppoat_conf_find_bool(conf, "server", &opt5);

	PPPOAT_ASSERT(rc == 0);

	PPPOAT_ASSERT(pppoat_streq(opt1, "pppd"));
	PPPOAT_ASSERT(pppoat_streq(opt2, "udp"));
	PPPOAT_ASSERT(pppoat_streq(opt3, "10.0.0.1:10.0.0.2"));
	PPPOAT_ASSERT(opt4 == 5000);
	PPPOAT_ASSERT(opt5);

	pppoat_free(opt1);
	pppoat_free(opt2);
	pppoat_free(opt3);

	rc = pppoat_conf_find_string_alloc(conf, "error", &opt1);
	PPPOAT_ASSERT(rc == -ENOENT);

	pppoat_conf_fini(conf);
	pppoat_free(conf);
}

struct pppoat_ut_group pppoat_tests_conf = {
	.ug_name = "conf",
	.ug_tests = {
		PPPOAT_UT_TEST("simple", ut_conf_simple),
		PPPOAT_UT_TEST("file", ut_conf_file),
		PPPOAT_UT_TEST_END,
	},
};
