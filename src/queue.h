/* queue.h
 * PPP over Any Transport -- Packet queue
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

#ifndef __PPPOAT_QUEUE_H__
#define __PPPOAT_QUEUE_H__

#include "list.h"
#include "mutex.h"

struct pppoat_packet;

struct pppoat_queue {
	struct pppoat_list  q_queue;
	struct pppoat_mutex q_lock;
};

int pppoat_queue_init(struct pppoat_queue *q);
void pppoat_queue_fini(struct pppoat_queue *q);
void pppoat_queue_enqueue(struct pppoat_queue *q, struct pppoat_packet *pkt);
struct pppoat_packet *pppoat_queue_dequeue(struct pppoat_queue *q);
struct pppoat_packet *pppoat_queue_dequeue_last(struct pppoat_queue *q);
struct pppoat_packet *pppoat_queue_front(struct pppoat_queue *q);
void pppoat_queue_pop_front(struct pppoat_queue *q);

#endif /* __PPPOAT_QUEUE_H__ */
