/* log.c
 * PPP over Any Transport -- Logging
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

#include "memory.h"

#include "log.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>

static pppoat_log_level_t log_level_min = PPPOAT_LOG_LEVEL_NR;
static struct pppoat_log_driver *log_drv = NULL;

static const char *log_level_name_tbl[PPPOAT_LOG_LEVEL_NR] = {
	[PPPOAT_DEBUG] = "DEBUG",
	[PPPOAT_INFO]  = "INFO ",
	[PPPOAT_ERROR] = "ERROR",
	[PPPOAT_FATAL] = "FATAL",
};

static const char *log_level_name(pppoat_log_level_t level)
{
	return level < PPPOAT_LOG_LEVEL_NR ?
	       log_level_name_tbl[level] : "NONE ";
}

int pppoat_log_init(struct pppoat_conf       *conf,
		    struct pppoat_log_driver *drv,
		    pppoat_log_level_t        level)
{
	int rc;

	rc = drv->ldrv_log == NULL ? -EINVAL : 0;
	if (drv->ldrv_init != NULL)
		rc = rc ?: drv->ldrv_init(drv, conf);
	if (rc == 0) {
		log_level_min = level;
		log_drv = drv;
	}
	return rc;
}

void pppoat_log_fini(void)
{
	struct pppoat_log_driver *drv = log_drv;

	/* 
	 * Disable logging first and only then finalise log driver.
	 * Avoids crashes when someone tries to log during finalisation.
	 */
	log_level_min = PPPOAT_LOG_LEVEL_NR;
	pppoat_log_flush();
	log_drv = NULL;
	if (drv->ldrv_fini != NULL)
		drv->ldrv_fini(drv);
}

void pppoat_log_flush(void)
{
	struct pppoat_log_driver *drv = log_drv;

	if (drv->ldrv_flush != NULL)
		drv->ldrv_flush(drv);
}

#define LOG_PREFIX_F "%s %s: "
#define LOG_PREFIX_P(level, area) level, area

void pppoat_log(pppoat_log_level_t level, const char *area,
		const char *fmt, ...)
{
	const char *slevel = log_level_name(level);
	char       *s;
	va_list     ap;
	size_t      total;
	int         len;

	if (level < log_level_min || log_drv == NULL)
		return;

	va_start(ap, fmt);

	/* Prepare buffer with enough space */

	len = snprintf(NULL, 0, LOG_PREFIX_F, LOG_PREFIX_P(slevel, area));
	if (len < 0)
		goto exit;
	total = (size_t)len;
	len = vsnprintf(NULL, 0, fmt, ap);
	va_end(ap);
	va_start(ap, fmt);
	if (len < 0)
		goto exit;
	total += (size_t)len;
	total += 1;
	s = pppoat_alloc(total);
	if (s == NULL)
		goto exit;

	/* Fill the buffer and send it to the driver */

	len = snprintf(s, total, LOG_PREFIX_F, LOG_PREFIX_P(slevel, area));
	if (len < 0 || len >= total)
		goto exit_free;
	total -= (size_t)len;
	len = vsnprintf(s + len, total, fmt, ap);
	if (len < 0 || len >= total)
		goto exit_free;

	log_drv->ldrv_log(log_drv, s);

exit_free:
	pppoat_free(s);
exit:
	va_end(ap);
}

static void log_driver_stderr_flush(struct pppoat_log_driver *drv)
{
	(void)fflush(stderr);
}

static void log_driver_stderr_log(struct pppoat_log_driver *drv,
				  const char               *msg)
{
	fprintf(stderr, "%s\n", msg);
}

struct pppoat_log_driver pppoat_log_driver_stderr = {
	.ldrv_name  = "stderr",
	.ldrv_init  = NULL,
	.ldrv_fini  = NULL,
	.ldrv_flush = &log_driver_stderr_flush,
	.ldrv_log   = &log_driver_stderr_log,
};
