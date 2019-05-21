/* thread.c
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

#include "trace.h"

#include "magic.h"
#include "thread.h"

#include <pthread.h>
#include <stdbool.h>

bool pppoat_thread_invariant(struct pppoat_thread *thread)
{
	return thread->t_magic == PPPOAT_THREAD_MAGIC;
}

int pppoat_thread_init(struct pppoat_thread *thread, pppoat_thread_func_t func)
{
	thread->t_func     = func;
	thread->t_userdata = NULL;
	thread->t_magic    = PPPOAT_THREAD_MAGIC;

	return 0;
}

void pppoat_thread_fini(struct pppoat_thread *thread)
{
}

static void *pthread_start_routine(void *arg)
{
	struct pppoat_thread *thread = arg;

	PPPOAT_ASSERT(pppoat_thread_invariant(thread));

	thread->t_func(thread);

	return NULL;
}

int pppoat_thread_start(struct pppoat_thread *thread)
{
	int rc;

	rc = pthread_create(&thread->t_thread, NULL,
			    &pthread_start_routine, thread);
	rc = rc == 0 ? 0 : P_ERR(-rc);

	return rc;
}

int pppoat_thread_join(struct pppoat_thread *thread)
{
	void *retval;
	int   rc;

	rc = pthread_join(thread->t_thread, &retval);
	rc = rc == 0 ? 0 : P_ERR(-rc);

	PPPOAT_ASSERT(rc != 0 || retval ==  NULL || retval == PTHREAD_CANCELED);
	if (rc == 0 && retval == PTHREAD_CANCELED)
		pppoat_debug("thread", "Thread %p was cancelled", thread);

	return rc;
}
