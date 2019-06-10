/* misc.h
 * PPP over Any Transport -- Helper routines and macros
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

#ifndef __PPPOAT_MISC_H__
#define __PPPOAT_MISC_H__

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

#ifndef container_of
#define container_of(ptr, type, member) \
		((type *)((char *)(ptr) - (char *)(&((type *)0)->member)))
#endif

/** Returns true iff string s1 and s2 are equal. */
#define pppoat_streq(s1, s2) (strcmp((s1), (s2)) == 0)

/**
 * Implication.
 *
 * In other words, if p is true, then q is also true. This macro is intended
 * to be used in conditions, especially in assertions like PPPOAT_ASSERT().
 */
#define imply(p, q) (!(p) || (q))

/**
 * Converts string to long.
 *
 * If the entire string can be converted and the result can be represented by
 * long, the result is returned in `out'. Otherwise, conversion fails.
 *
 * @return 0 on success or negative error code otherwise.
 */
int pppoat_strtol(char *str, long *out);

#endif /* __PPPOAT_MISC_H__ */
