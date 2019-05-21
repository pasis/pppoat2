/* trace.h
 * PPP over Any Transport -- Tracing, asserts, core dumps
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

#ifndef __PPPOAT_TRACE_H__
#define __PPPOAT_TRACE_H__

#include "log.h"

#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>

/* 
 * TODO It is a good idea to finalise log subsystem before abort(3).
 * We want to save as much logs as possible on crash. And some log drivers may
 * implement deferred logic.
 */

#define TRACE_LOC_F "%s:%d: %s():"
#define TRACE_LOC_P __FILE__, __LINE__, __func__

#ifdef NDEBUG
#define PPPOAT_TRACE_NOOP do {} while (0)
#define PPPOAT_ASSERT_INFO(expr, fmt, ...) PPPOAT_TRACE_NOOP
#define PPPOAT_ASSERT(expr)                PPPOAT_TRACE_NOOP
#define P_ERR(error)                       (error)
#else /* NDEBUG */

#define P_ERR(error)                                          \
	({                                                    \
		int __perror = (error);                       \
		pppoat_error("trace", TRACE_LOC_F" error=%d", \
			     TRACE_LOC_P, __perror);          \
		__perror;                                     \
	})

#define PPPOAT_ASSERT_INFO(expr, fmt, ...)                                     \
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

#define PPPOAT_ASSERT(expr) PPPOAT_ASSERT_INFO(expr, NULL)

#endif /* NDEBUG */

#endif /* __PPPOAT_TRACE_H__ */
