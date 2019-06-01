/* conf_argv.c
 * PPP over Any Transport -- Configuration (argc/argv source)
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
#include "misc.h"	/* ARRAY_SIZE */

#include <stdio.h>	/* printf */
#include <string.h>	/* strchr */
#include <unistd.h>	/* getopt */
#include <getopt.h>	/* getopt_long */

struct conf_argv_option {
	const char *cao_opt_long;
	int         cao_opt_short;
	bool        cao_argument;
	const char *cao_descr;
};

static const struct conf_argv_option conf_argv_opts[] = {
	{ "help",	'h', false, "Print help message" },
	{ "config",	'c', true,  "Read configuration from the file" },
	{ "interface",	'i', true,  "Interface module" },
	{ "transport",	't', true,  "Transport module" },
	{ "server",	's', false, "Server side" },
	{ "list",	'l', false, "Print list of supported modules" },
	{ "verbose",	'v', false, "Print debug messages" },
};

static char *conf_optstring(void)
{
	size_t  i;
	size_t  j;
	size_t  size = 0;
	char   *str;

	for (i = 0; i < ARRAY_SIZE(conf_argv_opts); ++i) {
		if (conf_argv_opts[i].cao_opt_short != 0) {
			++size;
			if (conf_argv_opts[i].cao_argument)
				++size;
		}
	}
	str = pppoat_alloc(size + 1);
	if (str == NULL)
		return NULL;
	for (i = 0, j = 0; i < ARRAY_SIZE(conf_argv_opts); ++i) {
		if (conf_argv_opts[i].cao_opt_short != 0) {
			str[j++] = conf_argv_opts[i].cao_opt_short;;
			if (conf_argv_opts[i].cao_argument)
				str[j++] = ':';
		}
	}
	str[j] = '\0';

	return str;
}

static struct option *conf_long_options(void)
{
	struct option *opts;
	size_t         count;
	size_t         i;

	count = ARRAY_SIZE(conf_argv_opts);
	opts = pppoat_alloc(sizeof *opts * (count + 1));
	if (opts == NULL)
		return NULL;

	for (i = 0; i < count; ++i) {
		opts[i] = (struct option){
			conf_argv_opts[i].cao_opt_long,
			conf_argv_opts[i].cao_argument ? required_argument :
							 no_argument,
			NULL,
			conf_argv_opts[i].cao_opt_short
		};
	}
	opts[count] = (struct option){ 0, 0, 0, 0 };

	return opts;
}

void pppoat_conf_print_usage(int argc, char **argv)
{
	const char *prog = "pppoat";
	size_t      i;

	if (argc > 0)
		prog = argv[0];

	printf("Usage: %s [OPTION]... [CONF]...\n\n", prog);
	printf("OPTIONS\n");
	for (i = 0; i < ARRAY_SIZE(conf_argv_opts); ++i) {
		if (conf_argv_opts[i].cao_opt_short != 0)
			printf("  -%c, ", conf_argv_opts[i].cao_opt_short);
		else
			printf("       ");
		printf("--%s", conf_argv_opts[i].cao_opt_long);
		if (conf_argv_opts[i].cao_argument)
			printf("=<VALUE>");
		printf("\t");
		if (!conf_argv_opts[i].cao_argument)
			printf("\t\t");
		printf("%s", conf_argv_opts[i].cao_descr);
		printf("\n");
	}
	printf("\n");
	printf("CONFIGURATION\n");
	printf("  Configuration is a list of key-values separated by '='. "
	       "Each module may have own specific configuration. "
	       "Please, refer to the module specific documentation or examples.\n");
}

static int conf_argv_store_single(struct pppoat_conf *conf,
				  const char         *key,
				  const char         *val,
				  bool                option)
{
	return pppoat_conf_store(conf, key, val);
}

int pppoat_conf_read_argv(struct pppoat_conf  *conf,
			  int                  argc,
			  char               **argv)
{
	struct option *opts_long = conf_long_options();
	char          *optstring = conf_optstring();
	char          *key;
	char          *val;
	int            index;
	int            c;
	int            i;
	int            rc = 0;

	if (opts_long == NULL || optstring == NULL) {
		rc = P_ERR(-ENOMEM);
		goto exit;
	}

	while (rc == 0) {
		c = getopt_long(argc, argv, optstring, opts_long, &index);
		if (c == -1 || c == '?')
			break;
		if (c == ':') {
			rc = P_ERR(-EINVAL);
			goto exit;
		}

		if (c != 0) {
			for (i = 0; i < ARRAY_SIZE(conf_argv_opts); ++i)
				if (conf_argv_opts[i].cao_opt_short == c)
					index = i;
		}
		key = (char *)conf_argv_opts[index].cao_opt_long;
		val = conf_argv_opts[index].cao_argument ? optarg : "1";
		rc = conf_argv_store_single(conf, key, val, true);
	}
	while (rc == 0 && optind < argc) {
		if (*argv[optind] == '-') {
			rc = P_ERR(-EINVAL);
			goto exit;
		}
		key = pppoat_strdup(argv[optind]);
		if (key == NULL) {
			rc = P_ERR(-ENOMEM);
			goto exit;
		}
		val = strchr(key, '=');
		if (val != NULL)
			*(val++) = '\0';
		else
			val = "1";
		rc = conf_argv_store_single(conf, key, val, false);
		pppoat_free(key);
		++optind;
	}

exit:
	pppoat_free(optstring);
	pppoat_free(opts_long);

	return rc;
}
