/* packet.c
 * PPP over Any Transport -- Packets interface
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
#include "misc.h"
#include "memory.h"
#include "packet.h"

static struct pppoat_list_descr packets_cache_descr =
	PPPOAT_LIST_DESCR("Packets cache", struct pppoat_packet, pkt_cache_link,
			  pkt_cache_magic, PPPOAT_PACKETS_CACHE_MAGIC);

static void packet_fini(struct pppoat_packet *pkt);

int pppoat_packets_init(struct pppoat_packets *pkts)
{
	pppoat_mutex_init(&pkts->pks_lock);
	pppoat_list_init(&pkts->pks_cache, &packets_cache_descr);
	pppoat_list_init(&pkts->pks_cache_empty, &packets_cache_descr);

	return 0;
}

static void packets_flush(struct pppoat_packets *pkts)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&pkts->pks_lock);
	while (!pppoat_list_is_empty(&pkts->pks_cache_empty)) {
		pkt = pppoat_list_pop(&pkts->pks_cache_empty);
		pppoat_free(pkt);
	}
	while (!pppoat_list_is_empty(&pkts->pks_cache)) {
		pkt = pppoat_list_pop(&pkts->pks_cache);
		packet_fini(pkt);
		pppoat_free(pkt);
	}
	pppoat_mutex_unlock(&pkts->pks_lock);
}

void pppoat_packets_fini(struct pppoat_packets *pkts)
{
	packets_flush(pkts);
	pppoat_list_fini(&pkts->pks_cache_empty);
	pppoat_list_fini(&pkts->pks_cache);
	pppoat_mutex_fini(&pkts->pks_lock);
}

static void packet_init(struct pppoat_packet *pkt)
{
	pkt->pkt_type = PPPOAT_PACKET_UNKNOWN;
	pkt->pkt_size = 0;
	pkt->pkt_size_actual = 0;
	pkt->pkt_data = NULL;
	pkt->pkt_ops = NULL;
	pkt->pkt_userdata = NULL;
}

static void packet_fini(struct pppoat_packet *pkt)
{
	const struct pppoat_packet_ops *ops = pkt->pkt_ops;

	if (ops != NULL && ops->pko_free != NULL)
		ops->pko_free(pkt);
}

static struct pppoat_packet *packet_create(size_t size)
{
	struct pppoat_packet *pkt;

	/*
	 * In the future, packets may be allocated with different
	 * allocators. For example, aligned allocator, or allocator
	 * which allows to use DMA.
	 * pkt->pkt_ops must be set with proper ops vector.
	 */

	pkt = pppoat_alloc(sizeof *pkt);
	if (pkt == NULL)
		return NULL;

	packet_init(pkt);
	if (size != 0) {
		/* Use standard allocator for now. */
		pkt->pkt_data = pppoat_alloc(size);
		pkt->pkt_size = size;
		pkt->pkt_size_actual = size;
		pkt->pkt_ops = &pppoat_packet_ops_std;

		if (pkt->pkt_data == NULL) {
			pppoat_free(pkt);
			pkt = NULL;
		}
	}
	return pkt;
}

struct pppoat_packet *pppoat_packet_get(struct pppoat_packets *pkts,
					size_t                 size)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&pkts->pks_lock);
	for (pkt = pppoat_list_head(&pkts->pks_cache);
	     pkt != NULL && pkt->pkt_size_actual < size;
	     pkt = pppoat_list_next(&pkts->pks_cache, pkt));
	if (pkt != NULL)
		pppoat_list_del(&pkts->pks_cache, pkt);
	pppoat_mutex_unlock(&pkts->pks_lock);

	if (pkt == NULL)
		pkt = packet_create(size);
	else
		pkt->pkt_size = size;

	return pkt;
}

struct pppoat_packet *pppoat_packet_get_empty(struct pppoat_packets *pkts)
{
	struct pppoat_packet *pkt;

	pppoat_mutex_lock(&pkts->pks_lock);
	pkt = pppoat_list_pop(&pkts->pks_cache_empty);
	pppoat_mutex_unlock(&pkts->pks_lock);
	if (pkt == NULL)
		pkt = packet_create(0);

	return pkt;
}

void pppoat_packet_put(struct pppoat_packets *pkts, struct pppoat_packet *pkt)
{
	if (pkt->pkt_size_actual == 0) {
		packet_fini(pkt);
		packet_init(pkt);
	} else {
		pkt->pkt_type = PPPOAT_PACKET_UNKNOWN;
		pkt->pkt_size = pkt->pkt_size_actual;
		pkt->pkt_userdata = NULL;
	}
	PPPOAT_ASSERT(imply(pkt->pkt_size_actual == 0, pkt->pkt_data == NULL));

	pppoat_mutex_lock(&pkts->pks_lock);
	if (pkt->pkt_size_actual == 0)
		pppoat_list_push(&pkts->pks_cache_empty, pkt);
	else
		pppoat_list_push(&pkts->pks_cache, pkt);
	pppoat_mutex_unlock(&pkts->pks_lock);
}

static void packet_ops_std_free(struct pppoat_packet *pkt)
{
	pppoat_free(pkt->pkt_data);
	pkt->pkt_data = NULL;
	pkt->pkt_size = 0;
}

const struct pppoat_packet_ops pppoat_packet_ops_std = {
	.pko_free = &packet_ops_std_free,
};
