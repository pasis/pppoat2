/* sem.h
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

#ifndef __PPPOAT_SEMAPHORE_H__
#define __PPPOAT_SEMAPHORE_H__

#include <stdbool.h>		/* bool */

#ifdef __APPLE__
#include <dispatch/dispatch.h>	/* dispatch_semaphore_t */
#else
#include <semaphore.h>		/* sem_t */
#endif

struct pppoat_semaphore {
#ifdef __APPLE__
	dispatch_semaphore_t s_sem;
#else
	sem_t s_sem;
#endif
};

void pppoat_semaphore_init(struct pppoat_semaphore *sem, unsigned int value);
void pppoat_semaphore_fini(struct pppoat_semaphore *sem);
void pppoat_semaphore_wait(struct pppoat_semaphore *sem);
bool pppoat_semaphore_trywait(struct pppoat_semaphore *sem);
void pppoat_semaphore_post(struct pppoat_semaphore *sem);

#endif /* __PPPOAT_SEMAPHORE_H__ */
