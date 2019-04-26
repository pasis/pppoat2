/* pppoat.c
 * PPP over Any Transport -- PPPoAT context
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
#include "memory.h"

#include "pppoat.h"

#include <stdio.h>	/* fprintf */
#include <stdlib.h>	/* exit */

static const pppoat_log_level_t default_log_level = PPPOAT_DEBUG;
static struct pppoat_log_driver * const default_log_drv =
						&pppoat_log_driver_stderr;

static void log_init_or_exit(struct pppoat_conf *conf)
{
	struct pppoat_log_driver *drv   = NULL;
	pppoat_log_level_t        level = 0;
	int                       rc;

	if (conf == NULL) {
		drv   = default_log_drv;
		level = default_log_level;
	}

	rc = pppoat_log_init(conf, drv, level);
	if (rc != 0) {
		/* We can't use pppoat_log(), just report to stderr. */
		fprintf(stderr, "Could not initialise %s log subsystem "
				"(rc=%d)", drv->ldrv_name, rc);
		exit(1);
	}
}

int main(int argc, char **argv)
{
	struct pppoat *ctx;
	int            rc;

	/* First, initialise default logger to catch logging on early stages. */
	log_init_or_exit(NULL);

	ctx = pppoat_alloc(sizeof(*ctx));
	PPPOAT_ASSERT(ctx != NULL);

	/* XXX Just print something for testing */
	pppoat_debug("main", "Test");
	pppoat_info("main", "ctx=%p", ctx);
	rc = P_ERR(-ENOENT);
	pppoat_fatal("main", "rc=%d", rc);

	pppoat_free(ctx);

	pppoat_log_fini();

	return 0;
}
