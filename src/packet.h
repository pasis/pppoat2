/* packet.h
 * PPP over Any Transport -- Packet interface
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

#ifndef __PPPOAT_PACKET_H__
#define __PPPOAT_PACKET_H__

#include "list.h"
#include "mutex.h"

struct pppoat_packet;

struct pppoat_packets {
	struct pppoat_list  pks_cache;
	struct pppoat_list  pks_cache_empty;
	struct pppoat_mutex pks_lock;
};

enum pppoat_packet_type {
	PPPOAT_PACKET_UNKNOWN,
	PPPOAT_PACKET_SEND,
	PPPOAT_PACKET_RECV,
};

struct pppoat_packet_ops {
	void (*pko_free)(struct pppoat_packet *pkt);
};

struct pppoat_packet {
	enum pppoat_packet_type         pkt_type;
	void                           *pkt_data;
	size_t                          pkt_size;
	size_t                          pkt_size_actual;
	const struct pppoat_packet_ops *pkt_ops;
	/** Link for queue/pipeline. */
	struct pppoat_list_link         pkt_q_link;
	uint32_t                        pkt_q_magic;
	/** Link for cache. */
	struct pppoat_list_link         pkt_cache_link;
	uint32_t                        pkt_cache_magic;
	/** Private userdata. */
	void                           *pkt_userdata;
};

/** Initialises packet subsystem. */
int pppoat_packets_init(struct pppoat_packets *pkts);
void pppoat_packets_fini(struct pppoat_packets *pkts);

/**
 * Returns an allocated packet with allocated data buffer.
 *
 * This get/put interface implements a cache of allocated packets. When
 * the cache is empty, get() allocates and returns new packet.
 *
 * The data buffer is at least `size' bytes. However, user may not access
 * memory over this size.
 *
 * @return Pointer to an empty packet object or NULL on memory allocation error.
 */
struct pppoat_packet *pppoat_packet_get(struct pppoat_packets *pkts,
					size_t                 size);

/**
 * Returns an allocated empty packet.
 *
 * @return Pointer to an empty packet object or NULL on memory allocation error.
 */
struct pppoat_packet *pppoat_packet_get_empty(struct pppoat_packets *pkts);

/**
 * Marks the packet object as unused.
 *
 * The packet may not be used after this call. It is not defined what happens
 * to the packet.
 */
void pppoat_packet_put(struct pppoat_packets *pkts, struct pppoat_packet *pkt);

extern const struct pppoat_packet_ops pppoat_packet_ops_std;

#endif /* __PPPOAT_PACKET_H__ */
