/* mutex.h
 * PPP over Any Transport -- Platform-independent mutex wrappers
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

#ifndef __PPPOAT_MUTEX_H__
#define __PPPOAT_MUTEX_H__

#include <pthread.h>	/* pthread_mutex_t */
#include <stdbool.h>	/* bool */

/**
 * Future work.
 *
 * There may be multiple implementations of the same locking primitive. For
 * example, pthread mutex and C11 mutex. Therefore, PPPoAT structures
 * should include one more abstraction layer:
 *
 * struct pppoat_mutex {
 * 	struct pppoat_mutex_impl m_mutex;
 * };
 *
 * struct pppoat_mutex_impl {
 * 	struct pthread_mutex_t mi_mutex;
 * 	or
 * 	mtx_t mi_mutex;
 * };
 *
 * Creating several pppoat_mutex classes is not an option, because it may
 * contain common fields in the future.
 *
 * Mutex can be improved in the next way:
 *
 * struct pppoat_mutex {
 * 	struct pppoat_mutex_impl  m_mutex;
 * 	const char               *m_name;
 * 	unsigned long             m_owner;
 * 	struct pppoat_list_link   m_link;
 * };
 *
 * All the new fields are supposed to make debugging easier. On a crash or
 * deadlock it is possible to iterate all locks in the system and print their
 * names, locking fact, and thread which owns the lock.
 * Owner is an identifier which allows to find proper thread in gdb. Also, this
 * field can be used to check whether mutex is locked by current thread or not.
 */

struct pppoat_mutex {
	pthread_mutex_t m_mutex;
};

void pppoat_mutex_init(struct pppoat_mutex *mutex);
void pppoat_mutex_fini(struct pppoat_mutex *mutex);
void pppoat_mutex_lock(struct pppoat_mutex *mutex);
void pppoat_mutex_unlock(struct pppoat_mutex *mutex);
bool pppoat_mutex_trylock(struct pppoat_mutex *mutex);

#endif /* __PPPOAT_MUTEX_H__ */
