/* memory.h
 * PPP over Any Transport -- Allocator
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

#ifndef __PPPOAT_MEMORY_H__
#define __PPPOAT_MEMORY_H__

#include <stddef.h>	/* size_t */

void *pppoat_alloc(size_t size);
void  pppoat_free(void *ptr);
void *pppoat_calloc(size_t nmemb, size_t size);
void *pppoat_realloc(void *ptr, size_t size);

char *pppoat_strdup(const char *s);

#endif /* __PPPOAT_MEMORY_H__ */
