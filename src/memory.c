/* memory.c
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

#include "memory.h"

#include <stdlib.h>

void *pppoat_alloc(size_t size)
{
	return malloc(size);
	/* TODO Poison. Objects must be initialised explicitly. */
}

void pppoat_free(void *ptr)
{
	/* TODO Poison. */
	free(ptr);
}

void *pppoat_calloc(size_t nmemb, size_t size)
{
	/* TODO Use pppoat_alloc(). */
	return calloc(nmemb, size);
}

void *pppoat_realloc(void *ptr, size_t size)
{
	return realloc(ptr, size);
}
