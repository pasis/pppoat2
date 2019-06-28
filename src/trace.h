/* trace.h
 * PPP over Any Transport -- Tracing, asserts, core dumps
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

#ifndef __PPPOAT_TRACE_H__
#define __PPPOAT_TRACE_H__

#include "log.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

#ifdef NDEBUG
#define PPPOAT_NDEBUG 1
#else
#define PPPOAT_NDEBUG 0
#endif /* NDEBUG */

#define TRACE_LOC_F "%s:%d: %s():"
#define TRACE_LOC_P __FILE__, __LINE__, __func__

#define pppoat_debug(area, fmt, ...)					     \
	do {								     \
		if (!PPPOAT_NDEBUG)					     \
			pppoat_log(PPPOAT_DEBUG, area, fmt, ## __VA_ARGS__); \
	} while (0)

#define pppoat_info(area, fmt, ...) \
	pppoat_log(PPPOAT_INFO, area, fmt, ## __VA_ARGS__)
#define pppoat_error(area, fmt, ...) \
	pppoat_log(PPPOAT_ERROR, area, fmt, ## __VA_ARGS__)
#define pppoat_fatal(area, fmt, ...) \
	pppoat_log(PPPOAT_FATAL, area, fmt, ## __VA_ARGS__)

/**
 * Prints error code and returns the code.
 *
 * Use this macro in a place where an error first occurs. For example:
 *	if (ptr == NULL)
 *		return P_ERR(-ENOMEM);
 * or
 * 	fd = open(name, O_RDONLY);
 * 	rc = fd < 0 ? P_ERR(-errno) : 0;
 */
#if (PPPOAT_NDEBUG == 1)
#define P_ERR(error) (error)
#else
#define P_ERR(error)                                          \
	({                                                    \
		int __perror = (error);                       \
		pppoat_error("trace", TRACE_LOC_F" error=%d", \
			     TRACE_LOC_P, __perror);          \
		__perror;                                     \
	})
#endif /* PPPOAT_NDEBUG */

/**
 * Implementation of the assert macro.
 *
 * Use wrappers PPPOAT_ASSERT() and PPPOAT_ASSERT_INFO() instead of the current
 * macro. Depending on the build configuration, we may want to remove asserts
 * from the target binary and this task is for the wrappers.
 *
 * @todo It is a good idea to finalise log subsystem before abort(3).
 * We want to save as much logs as possible on crash. And some log drivers may
 * implement deferred logic.
 */
#define PPPOAT_ASSERT_INFO_IMPL(expr, fmt, ...)                                \
	do {                                                                   \
		const char *__fmt  = (fmt);                                    \
		bool        __expr = (expr);                                   \
		if (!__expr) {                                                 \
			pppoat_fatal("trace",                                  \
				     TRACE_LOC_F" Assertion `%s' failed"       \
				     " (errno=%d)",                            \
				     TRACE_LOC_P, # expr, errno);              \
			if (__fmt != NULL) {                                   \
				pppoat_fatal("trace", __fmt, ## __VA_ARGS__);  \
			}                                                      \
			pppoat_log_flush();                                    \
			abort();                                               \
		}                                                              \
	} while (0)

#define PPPOAT_ASSERT_INFO(expr, fmt, ...)				       \
	do {								       \
		if (!PPPOAT_NDEBUG)					       \
			PPPOAT_ASSERT_INFO_IMPL((expr), (fmt), ## __VA_ARGS__);\
	} while (0)

#define PPPOAT_ASSERT(expr) PPPOAT_ASSERT_INFO(expr, NULL)

#endif /* __PPPOAT_TRACE_H__ */
