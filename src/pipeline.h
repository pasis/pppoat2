/* pipeline.h
 * PPP over Any Transport -- Pipeline of modules
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

#ifndef __PPPOAT_PIPELINE_H__
#define __PPPOAT_PIPELINE_H__

#include "list.h"

#include <stdbool.h>

/**
 * High level design.
 *
 * Pipeline is a bidirectional list of modules.
 *
 * Every module can produce a packet which is moved through all the modules,
 * starting from the producer towards the most left or the most right module
 * in the pipeline. To implement packet traverse through a pipeline, modules
 * can process a packet.
 *
 * Also a module can consume the packet (drop it or defer) or modify it.
 * Consumption means that the module takes ownership of the packet and
 * responsibility to release reference to the packet when it's not needed
 * anymore. Pipeline remains the owner of packets until they are consumed.
 * If interface or transport don't consume a packet for some reason, the
 * pipeline releases reference to the packet when it comes to the end.
 *
 * Modules don't insert packets to pipeline themself. Instead, pipeline uses
 * polling approach. In such a way we achieve encapsulation and modules don't
 * know that they're used by a pipeline. Besides, pipeline can introduce
 * optimizations like multithreading without modification of the modules.
 *
 * Since modules don't produce packets continuously, we need a mechanism to
 * determine where to stop polling a specific module and when to start again.
 * Therefore, a module can return an "empty" packet which means that the module
 * doesn't have data to return. We need to introduce event-driven architecture
 * and a module should decide what events make the pipeline to continue polling
 * the paused module.
 *
 * Usual usecase is when interface and transport produce packets and they
 * traverse to the opposite side:
 *
 * @verbatim
 * pkt ->   --->   --->
 * I <--> P <--> P <--> T
 *   <---   <---   <- pkt
 *
 * I - interface
 * P - plugin
 * T - transport
 * pkt - packet
 * @endverbatim
 */

/**
 * Detailed level design.
 *
 * Module interface provides a single interface for the pipeline needs:
 *
 * @verbatim
 * int pppoat_module_ops::mop_process(struct pppoat_packet *pkt_in,
 * 				      struct pppoat_packet **pkt_out);
 * @endverbatim
 *
 * pkt_in and/or can be an empty packet which is NULL. Empty pkt_in means that
 * pipeline polls for a new packet if the module has data to insert to the
 * pipeline. Empty pkt_out informs that the module doesn't have data and
 * the pipeline should stop polling it until an event occurs.
 *
 * If pkt_in is not empty, module must perform its logic on the packet
 * and either return it via pkt_out or consume it. If the packet is consumed,
 * the module still may create and return new packet via pkt_out.
 */

/* TODO Rewrite pipeline and modules to reflect the design. */

struct pppoat_module;
struct pppoat_packet;

struct pppoat_pipeline {
	struct pppoat_list pl_modules;
	bool               pl_is_ready;
};

int pppoat_pipeline_init(struct pppoat_pipeline *p);
void pppoat_pipeline_fini(struct pppoat_pipeline *p);

void pppoat_pipeline_add_module(struct pppoat_pipeline *p,
				struct pppoat_module   *mod);

void pppoat_pipeline_ready(struct pppoat_pipeline *p, bool ready);

int pppoat_pipeline_packet_send(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt);
int pppoat_pipeline_packet_recv(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt);

#endif /* __PPPOAT_PIPELINE_H__ */
