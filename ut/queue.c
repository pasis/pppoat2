/* ut/queue.c
 * PPP over Any Transport -- Unit tests (Queue)
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

#include "memory.h"
#include "packet.h"
#include "queue.h"
#include "ut/ut.h"

static void ut_queue_simple(void)
{
	struct pppoat_queue   *q;
	struct pppoat_packets *pkts;
	struct pppoat_packet  *pkt;
	struct pppoat_packet  *pkt1;
	struct pppoat_packet  *pkt2;
	int                    rc;

	q    = pppoat_alloc(sizeof *q);
	pkts = pppoat_alloc(sizeof *pkts);
	PPPOAT_ASSERT(q != NULL && pkts != NULL);

	rc = pppoat_packets_init(pkts);
	PPPOAT_ASSERT(rc == 0);
	rc = pppoat_queue_init(q);
	PPPOAT_ASSERT(rc == 0);
	pkt1 = pppoat_packet_get_empty(pkts);
	PPPOAT_ASSERT(pkt1 != NULL);
	pkt2 = pppoat_packet_get_empty(pkts);
	PPPOAT_ASSERT(pkt2 != NULL);

	/* Empty queue. */

	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == NULL);

	/* Order of enqueue/dequeue. */

	pppoat_queue_enqueue(q, pkt1);
	pppoat_queue_enqueue(q, pkt2);

	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == pkt1);
	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == pkt2);
	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == NULL);

	/* Order of enqueue/dequeue_last. */

	pppoat_queue_enqueue(q, pkt1);
	pppoat_queue_enqueue(q, pkt2);

	pkt = pppoat_queue_dequeue_last(q);
	PPPOAT_ASSERT(pkt == pkt2);
	pkt = pppoat_queue_dequeue_last(q);
	PPPOAT_ASSERT(pkt == pkt1);
	pkt = pppoat_queue_dequeue_last(q);
	PPPOAT_ASSERT(pkt == NULL);

	/* Return back to the queue. */

	pppoat_queue_enqueue(q, pkt1);
	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == pkt1);
	pppoat_queue_enqueue(q, pkt1);
	pkt = pppoat_queue_dequeue(q);
	PPPOAT_ASSERT(pkt == pkt1);

	/* Finalisation. */

	pppoat_packet_put(pkts, pkt2);
	pppoat_packet_put(pkts, pkt1);
	pppoat_queue_fini(q);
	pppoat_packets_fini(pkts);

	pppoat_free(pkts);
	pppoat_free(q);
}

struct pppoat_ut_group pppoat_tests_queue = {
	.ug_name = "queue",
	.ug_tests = {
		PPPOAT_UT_TEST("simple", ut_queue_simple),
		PPPOAT_UT_TEST_END,
	},
};
