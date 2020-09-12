/* thread.h
 * PPP over Any Transport -- Threads
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

#ifndef __PPPOAT_THREAD_H__
#define __PPPOAT_THREAD_H__

#include <pthread.h>	/* pthread_t */
#include <stdint.h>	/* uint32_t */

struct pppoat_thread;

typedef void (*pppoat_thread_func_t)(struct pppoat_thread *);

struct pppoat_thread {
	pthread_t             t_thread;
	pppoat_thread_func_t  t_func;
	void                 *t_userdata;
	uint32_t              t_magic;
};

int pppoat_thread_init(struct pppoat_thread *thread, pppoat_thread_func_t func);
void pppoat_thread_fini(struct pppoat_thread *thread);

int pppoat_thread_start(struct pppoat_thread *thread);
int pppoat_thread_join(struct pppoat_thread *thread);
int pppoat_thread_cancel(struct pppoat_thread *thread);

#endif /* __PPPOAT_THREAD_H__ */
