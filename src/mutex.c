/* mutex.c
 * PPP over Any Transport -- Platform-independent mutex wrappers
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

#include "mutex.h"

void pppoat_mutex_init(struct pppoat_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_init(&mutex->m_mutex, NULL);
	PPPOAT_ASSERT(rc == 0);
}

void pppoat_mutex_fini(struct pppoat_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_destroy(&mutex->m_mutex);
	PPPOAT_ASSERT(rc == 0);
}

void pppoat_mutex_lock(struct pppoat_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_lock(&mutex->m_mutex);
	PPPOAT_ASSERT(rc == 0);
}

void pppoat_mutex_unlock(struct pppoat_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_unlock(&mutex->m_mutex);
	PPPOAT_ASSERT(rc == 0);
}

bool pppoat_mutex_trylock(struct pppoat_mutex *mutex)
{
	int rc;

	rc = pthread_mutex_trylock(&mutex->m_mutex);
	PPPOAT_ASSERT((rc == 0) || (rc == EBUSY));

	return (rc == 0);
}
