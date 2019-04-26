/* conf.c
 * PPP over Any Transport -- Configuration
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

#include "conf.h"

int pppoat_conf_init(struct pppoat_conf *conf)
{
	return 0;
}

void pppoat_conf_fini(struct pppoat_conf *conf)
{
}

void pppoat_conf_flush(struct pppoat_conf *conf)
{
}

int pppoat_conf_store(struct pppoat_conf *conf,
		      const char         *key,
		      const char         *val)
{
	return 0;
}

void pppoat_conf_drop(struct pppoat_conf *conf, const char *key)
{
}

struct pppoat_conf_record *pppoat_conf_lookup(struct pppoat_conf *conf,
					      const char         *key)
{
	return NULL;
}

void pppoat_conf_record_get(struct pppoat_conf_record *record)
{
}

void pppoat_conf_record_put(struct pppoat_conf_record *record)
{
}

int pppoat_conf_find_long(struct pppoat_conf *conf,
			  const char         *key,
			  long               *out)
{
	return 0;
}

int pppoat_conf_find_string(struct pppoat_conf *conf,
			    const char         *key,
			    char               *out,
			    size_t              outlen)
{
	return 0;
}

int pppoat_conf_find_string_alloc(struct pppoat_conf  *conf,
				  const char          *key,
				  char               **out)
{
	return 0;
}

/*
 * Configuration iterator.
 */

int pppoat_conf_iter_init(struct pppoat_conf_iter *iter,
			  struct pppoat_conf      *conf,
			  const char              *prefix,
			  bool                     sort)
{
	return 0;
}

void pppoat_conf_iter_fini(struct pppoat_conf_iter *iter)
{
}

const struct pppoat_keyval *
pppoat_conf_iter_next(struct pppoat_conf_iter *iter)
{
	return NULL;
}

bool pppoat_conf_iter_is_end(struct pppoat_conf_iter *iter)
{
	return false;
}

/*
 * Configuration sources.
 */

int pppoat_conf_read_argv(struct pppoat_conf  *conf,
			  int                  argc,
			  const char         **argv)
{
	return 0;
}

int pppoat_conf_read_file(struct pppoat_conf *conf, const char *filename)
{
	return 0;
}
