/* lock.h
 * PPP over Any Transport -- Locking primitives
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

#ifndef __PPPOAT_LOCK_H__
#define __PPPOAT_LOCK_H__

#include <pthread.h>	/* pthread_mutex_t */
#include <stdbool.h>	/* bool */

struct pppoat_mutex {
	pthread_mutex_t m_mutex;
};

void pppoat_mutex_init(struct pppoat_mutex *mutex);
void pppoat_mutex_fini(struct pppoat_mutex *mutex);
void pppoat_mutex_lock(struct pppoat_mutex *mutex);
void pppoat_mutex_unlock(struct pppoat_mutex *mutex);
bool pppoat_mutex_trylock(struct pppoat_mutex *mutex);

#endif /* __PPPOAT_LOCK_H__ */
