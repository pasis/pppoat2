/* conf_file.c
 * PPP over Any Transport -- Configuration (file source)
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

#include "conf.h"
#include "memory.h"
#include "misc.h"	/* pppoat_streq */

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>	/* strlen, strcpy, strcat */

enum {
	CONF_FILE_LINE_MAX = 256,
};

static bool is_whitespace(char c)
{
	return c == ' ' || c == '\t' || c == '\n' || c == '\r';
}

static char *conf_file_trim(char *s)
{
	size_t len;

	len = strlen(s);
	for (; len > 0 && is_whitespace(*s); s++, len--);
	while (len > 0 && is_whitespace(s[len - 1]))
		s[--len] = '\0';

	return s;
}

static char *conf_file_key(char *section, char *sfx)
{
	char   *key;
	size_t  len;

	len = strlen(sfx);
	if (section == NULL || pppoat_streq(section, "core"))
		section = "";
	else
		len += strlen(section) + 1;

	key = pppoat_alloc(len + 1);
	if (key != NULL) {
		key[0] = '\0';
		if (*section != '\0') {
			strcpy(key, section);
			strcat(key, ".");
		}
		strcat(key, sfx);
	}
	return key;
}

static int conf_file_store(struct pppoat_conf *conf, char *key, char *val)
{
	struct pppoat_conf_record *r;
	int                        rc;

	r = pppoat_conf_lookup(conf, key);
	if (r != NULL) {
		pppoat_conf_record_put(r);
		/*
		 * If the key exists, it is not an error. File source has
		 * lower priority than other sources (e.g. argv source),
		 * but in some scenarios is processed after them.
		 */
		rc = 0;
	} else
		rc = pppoat_conf_store(conf, key, val);

	return rc;
}

int pppoat_conf_read_file(struct pppoat_conf *conf, const char *filename)
{
	char   *section = NULL;
	char   *line;
	char   *s;
	char   *key;
	char   *val;
	FILE   *f;
	size_t  len;
	int     rc;

	line = pppoat_alloc(CONF_FILE_LINE_MAX);
	if (line == NULL)
		return P_ERR(-ENOMEM);

	f = fopen(filename, "r");
	if (f == NULL) {
		pppoat_free(line);
		return P_ERR(-errno);
	}

	do {
		s = fgets(line, CONF_FILE_LINE_MAX, f);
		if (s == NULL) {
			if (!feof(f) && ferror(f) != 0)
				rc = P_ERR(-EIO);
			break;
		}

		s = conf_file_trim(s);
		len = strlen(s);
		if (*s == '[' && s[len - 1] == ']') {
			s[len - 1] = '\0';
			s = conf_file_trim(s + 1);
			pppoat_free(section);
			section = pppoat_strdup(s);
			if (section == NULL) {
				rc = P_ERR(-ENOMEM);
				break;
			}
		} else if (*s == '\0' || *s == '#') {
			/* Empty lines and comments. */
			continue;
		} else {
			val = strchr(s, '=');
			if (val == NULL) {
				pppoat_error("conf", "Can't parse line:"
						     "%s", s);
				rc = P_ERR(-EINVAL);
				break;
			}
			*val = '\0';
			s   = conf_file_trim(s);
			key = conf_file_key(section, s);
			val = conf_file_trim(val + 1);
			if (key == NULL) {
				rc = P_ERR(-ENOMEM);
				break;
			}
			rc = conf_file_store(conf, key, val);
			pppoat_free(key);
		}
	} while (rc == 0);

	pppoat_free(section);
	fclose(f);
	pppoat_free(line);

	return rc;
}
