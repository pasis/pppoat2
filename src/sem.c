/* sem.c
 * PPP over Any Transport -- Platform-independent semaphore wrappers
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

#include "misc.h"	/* imply */
#include "sem.h"

#ifdef __APPLE__

void pppoat_semaphore_init(struct pppoat_semaphore *sem, unsigned int value)
{
	sem->s_sem = dispatch_semaphore_create(value);
}

void pppoat_semaphore_fini(struct pppoat_semaphore *sem)
{
}

void pppoat_semaphore_wait(struct pppoat_semaphore *sem)
{
	long rc;

	rc = dispatch_semaphore_wait(sem->s_sem, DISPATCH_TIME_FOREVER);

	PPPOAT_ASSERT(rc == 0);
}

bool pppoat_semaphore_trywait(struct pppoat_semaphore *sem)
{
	long rc;

	rc = dispatch_semaphore_wait(sem->s_sem, DISPATCH_TIME_NOW);

	return rc == 0;
}

void pppoat_semaphore_post(struct pppoat_semaphore *sem)
{
	dispatch_semaphore_signal(sem->s_sem);
}

/* -------------------------------------------------------------------------- */
#else /* __APPLE__ */

void pppoat_semaphore_init(struct pppoat_semaphore *sem, unsigned int value)
{
	int rc;

	rc = sem_init(&sem->s_sem, 0, value);
	PPPOAT_ASSERT(rc == 0);
}

void pppoat_semaphore_fini(struct pppoat_semaphore *sem)
{
	int rc;

	rc = sem_destroy(&sem->s_sem);
	PPPOAT_ASSERT(rc == 0);
}

void pppoat_semaphore_wait(struct pppoat_semaphore *sem)
{
	int rc;

	do {
		rc = sem_wait(&sem->s_sem);
	} while (rc == -1 && errno == EINTR);

	PPPOAT_ASSERT(rc == 0);
}

bool pppoat_semaphore_trywait(struct pppoat_semaphore *sem)
{
	int rc;

	do {
		rc = sem_trywait(&sem->s_sem);
	} while (rc == -1 && errno == EINTR);

	PPPOAT_ASSERT(imply(rc == -1, errno == EAGAIN));

	return !(rc == -1 && errno == EAGAIN);
}

void pppoat_semaphore_post(struct pppoat_semaphore *sem)
{
	int rc;

	rc = sem_post(&sem->s_sem);
	PPPOAT_ASSERT(rc == 0);
}

#endif /* __APPLE__ */
