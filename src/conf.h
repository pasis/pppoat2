/* conf.h
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

#ifndef __PPPOAT_CONF_H__
#define __PPPOAT_CONF_H__

#include "list.h"
#include "mutex.h"

#include <stdbool.h>	/* bool */
#include <stddef.h>	/* size_t */

/**
 * High level design.
 *
 * Configuration accumulates all known preferences about system and its modules.
 * It provides key-value like interface. An instance of configuration must hold
 * only records with unique keys. Key is a string of letters in lower case,
 * digits and dots.
 *
 * Users of the configuration subsystem are core program and modules. All users
 * have access to all records in configuration instance and may add new or
 * replace existent records. There may be multiple configuration instances and
 * they must not have relations.
 *
 * Configuration has flat structure. However, a user should distinguish their
 * keys from keys of other users. Users should follow the next convention:
 * core program configuration keys don't have to contain a prefix, modules'
 * ones should start with a prefix separated with ".". Also, users may
 * introduce keys with multiple prefixes, for example, "compress.lzma.ratio".
 *
 * There is no configuration scheme. An instance may contain records with
 * any acceptable keys. User may lookup for a non existing key and receives
 * error in this case. The error is an expected result and can be used to
 * ignore non mandatory options for example.
 *
 * There are multiple sources of preferences and all the sources must be
 * ordered. This is because keys may overlap in different sources and if it
 * happens, only record from source with the highest order (priority) is held
 * in the configuration.
 * For example, sources are command line options, local configuration file in
 * user's home directory, global configuration file. Order in this case is the
 * next: command line > local file > global file.
 *
 * A record's value can be returned as a string or as a number (if applicable).
 *
 * Expected usecase. Core program and modules fill configuration instance during
 * initialisation/startup in non concurrent way. After this, main pattern is not
 * frequent and mostly not concurrent lookup through the configuration.
 */

/**
 * Detailed level design.
 *
 * Record is a pair of key/value in string representation. All the records are
 * stored in a store inside configuration instance. The store is implemented as
 * list, however may be replaced with hash table in the future.
 *
 * Configuration record has a reference counter which guarantees that the record
 * is not freed until there is no active user.
 *
 * There are 3 main operations: STORE, DROP, LOOKUP.
 *  - STORE stores a record with given key/value within the store. If a record
 *    with the key already exists, it's replaced. Otherwise, new record is
 *    added. Replacing is implemented via DROP, therefore, if the old record
 *    has active users it won't be freed immediately.
 *  - DROP removes record from the store and releases its reference.
 *  - LOOKUP returns record by key. If a record with the key doesn't exist the
 *    operation returns NULL. NULL is a valid expected result of GET and should
 *    be handled by users. LOOKUP takes implicit reference which must be
 *    released by user.
 *
 * Configuration instance is initialised with generation 0 and it is increased
 * with every change to the store. This can be used for implementing complex
 * non atomic operations.
 *
 * Every access to the store and generation counter must be protected with a
 * lock.
 *
 * All interface except of init/fini is thread-safe. Init/fini calls must be
 * serialised by the core program and there must be no active users at the
 * moment when fini is called.
 */

struct pppoat_conf {
	struct pppoat_mutex  c_lock;
	/** Stores all records. */
	struct pppoat_list   c_store;
	/** Number of records in the store. */
	size_t               c_store_nr;
	/** Generation, increased with every update. */
	unsigned long        c_gen;
};

struct pppoat_conf_record {
	char                    *cr_key;
	char                    *cr_val;
	/* Reference counter. Use signed type to catch memory corruptions. */
	long                     cr_ref;
	struct pppoat_conf      *cr_conf;
	struct pppoat_list_link  cr_link;
	uint32_t                 cr_magic;
};

int pppoat_conf_init(struct pppoat_conf *conf);
void pppoat_conf_fini(struct pppoat_conf *conf);

/**
 * Drops all the records.
 *
 * Result of this function is empty configuration instance.
 */
void pppoat_conf_flush(struct pppoat_conf *conf);

int pppoat_conf_store(struct pppoat_conf *conf,
		      const char         *key,
		      const char         *val);
void pppoat_conf_drop(struct pppoat_conf *conf, const char *key);

/**
 * Searches for the record with given key.
 *
 * @note Takes implicit reference to the record which must be released with
 *       pppoat_conf_record_put().
 * @return Pointer to the record or NULL if it doesn't exist.
 */
struct pppoat_conf_record *pppoat_conf_lookup(struct pppoat_conf *conf,
					      const char         *key);

/** Get reference to the record. */
void pppoat_conf_record_get(struct pppoat_conf_record *r);
/** Release reference to the record. */
void pppoat_conf_record_put(struct pppoat_conf_record *r);

/**
 * Configuration wrappers.
 *
 * This interface provides logic over the main interface and doesn't depend
 * on internal knowledge of pppoat_conf structure.
 *
 * Users supposed to use wrappers, because reference counting of
 * pppoat_conf_record objects is source of mistakes.
 */

/**
 * Lookup for the record and return numeric value to the out argument.
 *
 * If such a record doesn't exist it returns -ENOENT. If the value can't be
 * represented as an integer number the function returns -EINVAL. These two
 * errors are expected results and should be handled by users.
 *
 * @return 0, -ENOENT or -EINVAL.
 */
int pppoat_conf_find_long(struct pppoat_conf *conf,
			  const char         *key,
			  long               *out);

/**
 * Lookup for the record and return string value to the user supplied buffer.
 *
 * If such a record doesn't exist it returns -ENOENT. If the value can't be
 * fully stored in the user's buffer -ERANGE is returned.
 *
 * @return 0, -ENOENT or -ERANGE.
 */
int pppoat_conf_find_string(struct pppoat_conf *conf,
			    const char         *key,
			    char               *out,
			    size_t              outlen);

/**
 * Lookup for the record and return string value as newly allocated string.
 *
 * The resulting string must be freed with pppoat_free().
 *
 * If such a record doesn't exist it returns -ENOENT. -ENOMEM is returned
 * on memory allocation error.
 *
 * @return 0, -ENOENT or -ENOMEM.
 */
int pppoat_conf_find_string_alloc(struct pppoat_conf  *conf,
				  const char          *key,
				  char               **out);

/**
 * Lookup for a boolean record.
 *
 * The result is false if either:
 *  - There is no record with specified key;
 *  - The value is 0;
 *  - The value is false, False or FALSE;
 *
 * Otherwise, the result is true.
 *
 * @note This function doesn't fail.
 */
void pppoat_conf_find_bool(struct pppoat_conf *conf,
			   const char         *key,
			   bool               *out);

/**
 * Parses argc/argv and stores records to the configuration instance.
 *
 * It supports both predefined options with dash and key/value separated with
 * "=". Format of the predefined options is hardcoded. First element of the
 * argv array is ignored due to convention.
 *
 * Command line options allow to specify configuration file (e.g.
 * "-c conf.ini"). In this case argv records will override ones from the
 * configuration file if they overlap.
 *
 * Example of the argv array:
 * @verbatim
 *   ./pppoat -c conf.ini -m tcp tcp.port=5555
 * @endverbatim
 *
 * @return 0 on success or negative error code otherwise.
 */
int pppoat_conf_read_argv(struct pppoat_conf  *conf,
			  int                  argc,
			  char               **argv);
void pppoat_conf_print_usage(int argc, char **argv);

/**
 * Reads configuration file in INI format and stores its content to the
 * configuration instance.
 *
 * @return 0 on success or negative error code otherwise.
 */
int pppoat_conf_read_file(struct pppoat_conf *conf, const char *filename);

/**
 * Configuration iterator.
 *
 * User can iterate through records with keys that have specified prefix.
 * Additionally, iteration process can be done in lexicographical order.
 *
 * Iterator allocates array of pointers to pppoat_conf_record objects and
 * uses it as a mapping to the configuration store. The array can hold
 * only subset of records and with a specific order.
 * Iterator takes reference to every record in the array.
 *
 * User may DROP or replace a record returned with an iterator.
 */

/** Simply key/value structure */
struct pppoat_keyval {
	const char *kv_key;
	const char *kv_val;
};

struct pppoat_conf_iter {
	struct pppoat_conf_record *ci_array;
	size_t                     ci_nr;
	size_t                     ci_pos;
	struct pppoat_keyval       ci_kv;
	struct pppoat_conf        *ci_conf;
	unsigned long              ci_conf_gen;
};

int pppoat_conf_iter_init(struct pppoat_conf_iter *iter,
			  struct pppoat_conf      *conf,
			  const char              *prefix,
			  bool                     sort);
void pppoat_conf_iter_fini(struct pppoat_conf_iter *iter);

const struct pppoat_keyval *
pppoat_conf_iter_next(struct pppoat_conf_iter *iter);
bool pppoat_conf_iter_is_end(struct pppoat_conf_iter *iter);

/**
 * Dump configuration.
 *
 * This function is used for testing purpose.
 */
void pppoat_conf_dump(struct pppoat_conf *conf);

#endif /* __PPPOAT_CONF_H__ */
