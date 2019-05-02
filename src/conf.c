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
#include "memory.h"
#include "magic.h"
#include "misc.h"	/* pppoat_strtol */

#include <string.h>	/* strcmp, strlen */

static struct pppoat_list_descr conf_store_descr =
	PPPOAT_LIST_DESCR("Conf store", struct pppoat_conf_record, cr_link,
			  cr_magic, PPPOAT_CONF_STORE_MAGIC);

static void conf_record_get_locked(struct pppoat_conf_record *r);
static void conf_record_put_locked(struct pppoat_conf_record *r);

int pppoat_conf_init(struct pppoat_conf *conf)
{
	pppoat_mutex_init(&conf->c_lock);
	pppoat_list_init(&conf->c_store, &conf_store_descr);
	conf->c_store_nr = 0;
	conf->c_gen = 0;

	return 0;
}

void pppoat_conf_fini(struct pppoat_conf *conf)
{
	pppoat_conf_flush(conf);
	pppoat_list_fini(&conf->c_store);
	pppoat_mutex_fini(&conf->c_lock);
}

void pppoat_conf_flush(struct pppoat_conf *conf)
{
	struct pppoat_conf_record *r;

	pppoat_mutex_lock(&conf->c_lock);
	while (!pppoat_list_is_empty(&conf->c_store)) {
		r = pppoat_list_pop(&conf->c_store);
		conf_record_put_locked(r);
	}
	conf->c_store_nr = 0;
	++conf->c_gen;
	pppoat_mutex_unlock(&conf->c_lock);
}

static struct pppoat_conf_record *conf_record_new(struct pppoat_conf *conf,
						  const char         *key,
						  const char         *val)
{
	struct pppoat_conf_record *r;

	r = pppoat_alloc(sizeof *r);
	if (r != NULL) {
		r->cr_key = pppoat_strdup(key);
		r->cr_val = pppoat_strdup(val);
		if (r->cr_key == NULL || r->cr_val == NULL)
			goto err_free;
		r->cr_conf = conf;
		r->cr_ref  = 1;
	}
	return r;

err_free:
	pppoat_free(r->cr_key);
	pppoat_free(r->cr_val);
	pppoat_free(r);
	return NULL;
}

static void conf_record_destroy(struct pppoat_conf_record *r)
{
	struct pppoat_conf *conf = r->cr_conf;

	PPPOAT_ASSERT(r->cr_ref == 0);

	/* Current function is called under conf->c_lock lock. */
	pppoat_list_del(&conf->c_store, r);

	pppoat_free(r->cr_key);
	pppoat_free(r->cr_val);
	pppoat_free(r);
}

int pppoat_conf_store(struct pppoat_conf *conf,
		      const char         *key,
		      const char         *val)
{
	struct pppoat_conf_record *r;

	r = conf_record_new(conf, key, val);
	if (r == NULL)
		return P_ERR(-ENOMEM);

	pppoat_mutex_lock(&conf->c_lock);
	pppoat_list_insert(&conf->c_store, r);
	++conf->c_gen;
	pppoat_mutex_unlock(&conf->c_lock);

	return 0;
}

void pppoat_conf_drop(struct pppoat_conf *conf, const char *key)
{
	struct pppoat_conf_record *r;

	r = pppoat_conf_lookup(conf, key);
	if (r != NULL) {
		/* Release reference gotten by LOOKUP. */
		pppoat_conf_record_put(r);
		/* Release reference gotten by conf_record_new(). */
		pppoat_conf_record_put(r);
	}
}

struct pppoat_conf_record *pppoat_conf_lookup(struct pppoat_conf *conf,
					      const char         *key)
{
	struct pppoat_conf_record *r;

	pppoat_mutex_lock(&conf->c_lock);
	for (r = pppoat_list_head(&conf->c_store); r != NULL;
	     r = pppoat_list_next(&conf->c_store, r)) {
		if (strcmp(key, r->cr_key) == 0)
			break;
	}
	if (r != NULL)
		conf_record_get_locked(r);
	pppoat_mutex_unlock(&conf->c_lock);

	return r;
}

static void conf_record_get_locked(struct pppoat_conf_record *r)
{
	/* If a reference == 0 we must not use the object. */
	PPPOAT_ASSERT(r->cr_ref > 0);

	++r->cr_ref;
}

void pppoat_conf_record_get(struct pppoat_conf_record *r)
{
	pppoat_mutex_lock(&r->cr_conf->c_lock);
	conf_record_get_locked(r);
	pppoat_mutex_unlock(&r->cr_conf->c_lock);
}

static void conf_record_put_locked(struct pppoat_conf_record *r)
{
	PPPOAT_ASSERT(r->cr_ref > 0);

	--r->cr_ref;
	if (r->cr_ref == 0)
		conf_record_destroy(r);
}

void pppoat_conf_record_put(struct pppoat_conf_record *r)
{
	pppoat_mutex_lock(&r->cr_conf->c_lock);
	conf_record_put_locked(r);
	pppoat_mutex_unlock(&r->cr_conf->c_lock);
}

int pppoat_conf_find_long(struct pppoat_conf *conf,
			  const char         *key,
			  long               *out)
{
	struct pppoat_conf_record *r;
	int                        rc;

	r  = pppoat_conf_lookup(conf, key);
	rc = r == NULL ? P_ERR(-ENOENT) : 0;
	if (rc == 0) {
		rc = pppoat_strtol(r->cr_val, out);
		pppoat_conf_record_put(r);
	}
	return rc;
}

int pppoat_conf_find_string(struct pppoat_conf *conf,
			    const char         *key,
			    char               *out,
			    size_t              outlen)
{
	struct pppoat_conf_record *r;
	size_t                     len;
	int                        rc;

	r  = pppoat_conf_lookup(conf, key);
	rc = r == NULL ? P_ERR(-ENOENT) : 0;
	if (rc == 0) {
		len = strlen(r->cr_val);
		if (len >= outlen)
			rc = P_ERR(-ERANGE);
		else
			strcpy(out, r->cr_val);
		pppoat_conf_record_put(r);
	}
	return rc;
}

int pppoat_conf_find_string_alloc(struct pppoat_conf  *conf,
				  const char          *key,
				  char               **out)
{
	struct pppoat_conf_record *r;
	char                      *val;
	size_t                     len;
	int                        rc;

	r  = pppoat_conf_lookup(conf, key);
	rc = r == NULL ? P_ERR(-ENOENT) : 0;
	if (rc == 0) {
		len = strlen(r->cr_val);
		val = pppoat_alloc(len + 1);
		if (val == NULL)
			rc = P_ERR(-ENOMEM);
		else
			strcpy(val, r->cr_val);
		pppoat_conf_record_put(r);
	}
	if (rc == 0)
		*out = val;

	return rc;
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
