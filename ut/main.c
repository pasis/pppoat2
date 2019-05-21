/* main.c
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
#include <stdlib.h>	/* exit */

static struct pppoat_ut ut_inst;

void add_all_tests(struct pppoat_ut *ut)
{
	extern struct pppoat_ut_group pppoat_tests_base64;
	extern struct pppoat_ut_group pppoat_tests_list;
	extern struct pppoat_ut_group pppoat_tests_conf;
	extern struct pppoat_ut_group pppoat_tests_packet;
	extern struct pppoat_ut_group pppoat_tests_queue;

	pppoat_ut_group_add(ut, &pppoat_tests_base64);
	pppoat_ut_group_add(ut, &pppoat_tests_list);
	pppoat_ut_group_add(ut, &pppoat_tests_conf);
	pppoat_ut_group_add(ut, &pppoat_tests_packet);
	pppoat_ut_group_add(ut, &pppoat_tests_queue);
}

int main(int argc, char **argv)
{
	int rc;

	rc = pppoat_log_init(NULL, &pppoat_log_driver_stderr, PPPOAT_DEBUG);
	if (rc != 0) {
		/* We can't use pppoat_log(), just report to stderr. */
		fprintf(stderr, "Could not initialise log subsystem (rc=%d)",
			rc);
		exit(1);
	}

	pppoat_ut_init(&ut_inst);
	add_all_tests(&ut_inst);
	pppoat_ut_run(&ut_inst);
	pppoat_ut_fini(&ut_inst);

	pppoat_log_fini();

	return 0;
}
