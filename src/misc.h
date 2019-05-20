/* misc.h
 * PPP over Any Transport -- Helper routines and macros
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

#ifndef __PPPOAT_MISC_H__
#define __PPPOAT_MISC_H__

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))
#endif

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
