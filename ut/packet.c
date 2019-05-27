/* ut/packet.c
 * PPP over Any Transport -- Unit tests (Packets)
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

#include "packet.h"
#include "ut/ut.h"

#include <string.h>	/* memset */

enum {
	UT_PACKET_SIZE = 1500,
	UT_PACKET_NR   = 5,
};

static struct pppoat_packets pkts;

static void ut_packet_get_empty(void)
{
	struct pppoat_packet *pkt;
	int                   rc;

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	pkt = pppoat_packet_get_empty(&pkts);
	PPPOAT_ASSERT(pkt != NULL);
	pppoat_packet_put(&pkts, pkt);
	pppoat_packets_fini(&pkts);
}

static void ut_packet_get_put(void)
{
	struct pppoat_packet *pkt;
	struct pppoat_packet *arr[UT_PACKET_NR];
	size_t                size;
	int                   i;
	int                   rc;

	/*
	 * Get several equal-size packets.
	 */

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	size = UT_PACKET_SIZE;
	for (i = 0; i < UT_PACKET_NR; ++i) {
		arr[i] = pppoat_packet_get(&pkts, size);
		PPPOAT_ASSERT(arr[i] != NULL);
		PPPOAT_ASSERT(arr[i]->pkt_data != NULL);
		PPPOAT_ASSERT(arr[i]->pkt_size >= size);
		memset(arr[i]->pkt_data, 0, size);
	}
	for (i = 0; i < UT_PACKET_NR; ++i)
		pppoat_packet_put(&pkts, arr[i]);
	pppoat_packets_fini(&pkts);

	/*
	 * Get and put packets with increasing size.
	 */

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	for (i = 0; i < UT_PACKET_NR; ++i) {
		size = UT_PACKET_SIZE + i;
		pkt = pppoat_packet_get(&pkts, size);
		PPPOAT_ASSERT(pkt != NULL);
		PPPOAT_ASSERT(pkt->pkt_data != NULL);
		PPPOAT_ASSERT(pkt->pkt_size >= size);
		memset(pkt->pkt_data, 0, size);
		pppoat_packet_put(&pkts, pkt);
	}
	pppoat_packets_fini(&pkts);

	/*
	 * Get and put packets with decreasing size.
	 */

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	for (i = 0; i < UT_PACKET_NR; ++i) {
		size = UT_PACKET_SIZE - i;
		pkt = pppoat_packet_get(&pkts, size);
		PPPOAT_ASSERT(pkt != NULL);
		PPPOAT_ASSERT(pkt->pkt_data != NULL);
		PPPOAT_ASSERT(pkt->pkt_size >= size);
		memset(pkt->pkt_data, 0, size);
		pppoat_packet_put(&pkts, pkt);
	}
	pppoat_packets_fini(&pkts);
}

static void ut_packet_ops_free_cb(struct pppoat_packet *pkt)
{
	int *counter = pkt->pkt_userdata;

	*counter += 1;
}

static void ut_packet_ops_free(void)
{
	struct pppoat_packet *pkt;
	int                   rc;

	static int            counter = 0;

	static const struct pppoat_packet_ops ops = {
		.pko_free = &ut_packet_ops_free_cb,
	};

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	pkt = pppoat_packet_get_empty(&pkts);
	PPPOAT_ASSERT(pkt != NULL);
	pkt->pkt_ops = &ops;
	pkt->pkt_userdata = &counter;
	pppoat_packet_put(&pkts, pkt);
	pppoat_packets_fini(&pkts);

	PPPOAT_ASSERT(counter == 1);
}

struct pppoat_ut_group pppoat_tests_packet = {
	.ug_name = "packet",
	.ug_tests = {
		PPPOAT_UT_TEST("get-empty", ut_packet_get_empty),
		PPPOAT_UT_TEST("get-put", ut_packet_get_put),
		PPPOAT_UT_TEST("ops-free", ut_packet_ops_free),
		PPPOAT_UT_TEST_END,
	},
};
