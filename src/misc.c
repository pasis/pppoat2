/* misc.c
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

#include "trace.h"

#include "misc.h"

#include <errno.h>
#include <stdlib.h>	/* strtol */

int pppoat_strtol(char *str, long *out)
{
	char *endptr;
	long  val;

	errno = 0;
	val = strtol(str, &endptr, 10);
	if (errno == ERANGE)
		return P_ERR(-ERANGE);
	if (*str == '\0' || *endptr != '\0')
		return P_ERR(-EINVAL);

	*out = val;
	return 0;
}
