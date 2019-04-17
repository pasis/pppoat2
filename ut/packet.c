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

static void ut_packet_init(void)
{
	struct pppoat_packet *pkt;
	int                   rc;

	static struct pppoat_packets pkts;

	rc = pppoat_packets_init(&pkts);
	PPPOAT_ASSERT(rc == 0);
	pkt = pppoat_packet_get(&pkts);
	PPPOAT_ASSERT(pkt != NULL);
	pppoat_packet_put(&pkts, pkt);
	pppoat_packets_fini(&pkts);
}

struct pppoat_ut_group pppoat_tests_packet = {
	.ug_name = "packet",
	.ug_tests = {
		PPPOAT_UT_TEST("init", ut_packet_init),
		PPPOAT_UT_TEST_END,
	},
};
