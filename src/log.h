/* log.h
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

#ifndef __PPPOAT_LOG_H__
#define __PPPOAT_LOG_H__

#include <stddef.h>	/* size_t */

struct pppoat_conf;
struct pppoat_log_driver;

typedef int (*pppoat_log_driver_init)(struct pppoat_log_driver *,
				      struct pppoat_conf *);
typedef void (*pppoat_log_driver_fini)(struct pppoat_log_driver *);
typedef void (*pppoat_log_driver_flush)(struct pppoat_log_driver *);
typedef void (*pppoat_log_driver_log)(struct pppoat_log_driver *, const char *);

struct pppoat_log_driver {
	const char              *ldrv_name;
	pppoat_log_driver_init   ldrv_init;
	pppoat_log_driver_fini   ldrv_fini;
	pppoat_log_driver_flush  ldrv_flush;
	pppoat_log_driver_log    ldrv_log;
	/** Private data, set and used by drivers */
	void                    *ldrv_priv;
};

typedef enum {
	PPPOAT_DEBUG,
	PPPOAT_INFO,
	PPPOAT_ERROR,
	PPPOAT_FATAL,
	PPPOAT_LOG_LEVEL_NR,
} pppoat_log_level_t;

int pppoat_log_init(struct pppoat_conf       *conf,
		    struct pppoat_log_driver *drv,
		    pppoat_log_level_t        level);
void pppoat_log_fini(void);

void pppoat_log_flush(void);

void pppoat_log(pppoat_log_level_t level, const char *area,
		const char *fmt, ...);

/**
 * Log a buffer as a string of hexadecimal numbers.
 *
 * @note Buffer is logged with PPPOAT_DEBUG level.
 */
void pppoat_log_hexdump(void *buf, size_t len);

/**
 * Simple log driver. Prints all messages to stderr.
 * This driver doesn't fail and has no dependencies.
 */
extern struct pppoat_log_driver pppoat_log_driver_stderr;

#endif /* __PPPOAT_LOG_H__ */
