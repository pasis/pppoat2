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

#include "trace.h"

#include "magic.h"
#include "packet.h"
#include "queue.h"

static struct pppoat_list_descr queue_descr =
	PPPOAT_LIST_DESCR("Packets queue", struct pppoat_packet, pkt_q_link,
			  pkt_q_magic, PPPOAT_QUEUE_MAGIC);

int pppoat_queue_init(struct pppoat_queue *q)
{
	pppoat_mutex_init(&q->q_lock);
	pppoat_list_init(&q->q_queue, &queue_descr);

	return 0;
}

void pppoat_queue_fini(struct pppoat_queue *q)
{
	pppoat_list_fini(&q->q_queue);
	pppoat_mutex_fini(&q->q_lock);
}

void pppoat_queue_enqueue(struct pppoat_queue *q, struct pppoat_packet *pkt)
{
	pppoat_mutex_lock(&q->q_lock);
	pppoat_list_enqueue(&q->q_queue, pkt);
	pppoat_mutex_unlock(&q->q_lock);
}

struct pppoat_packet *pppoat_queue_dequeue(struct pppoat_queue *q)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&q->q_lock);
	pkt = pppoat_list_dequeue(&q->q_queue);
	pppoat_mutex_unlock(&q->q_lock);

	return pkt;
}

struct pppoat_packet *pppoat_queue_dequeue_last(struct pppoat_queue *q)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&q->q_lock);
	pkt = pppoat_list_dequeue_last(&q->q_queue);
	pppoat_mutex_unlock(&q->q_lock);

	return pkt;
}

struct pppoat_packet *pppoat_queue_front(struct pppoat_queue *q)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&q->q_lock);
	pkt = pppoat_list_head(&q->q_queue);
	pppoat_mutex_unlock(&q->q_lock);

	return pkt;
}

void pppoat_queue_pop_front(struct pppoat_queue *q)
{
	pppoat_mutex_lock(&q->q_lock);
	(void)pppoat_list_pop(&q->q_queue);
	pppoat_mutex_unlock(&q->q_lock);
}
