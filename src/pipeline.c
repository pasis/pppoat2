/* pipeline.c
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

#include "trace.h"

#include "magic.h"
#include "module.h"
#include "packet.h"
#include "pipeline.h"

static struct pppoat_list_descr pipeline_descr =
	PPPOAT_LIST_DESCR("Pipeline", struct pppoat_module, m_link, m_magic,
			  PPPOAT_PIPELINE_MAGIC);

int pppoat_pipeline_init(struct pppoat_pipeline *p)
{
	p->pl_is_ready = false;
	pppoat_list_init(&p->pl_modules, &pipeline_descr);

	return 0;
}

static void pipeline_flush(struct pppoat_pipeline *p)
{
	while (!pppoat_list_is_empty(&p->pl_modules))
		(void)pppoat_list_pop(&p->pl_modules);
}

void pppoat_pipeline_fini(struct pppoat_pipeline *p)
{
	p->pl_is_ready = false;
	pipeline_flush(p);
	pppoat_list_fini(&p->pl_modules);
}

void pppoat_pipeline_add_module(struct pppoat_pipeline *p,
				struct pppoat_module   *mod)
{
	/*
	 * XXX TODO Add the following preconditions:
	 * 1. Interface module must be the 1st in the list.
	 * 2. For non interface module the head of the list must be an
	 *    interface module.
	 * 3. Plugins must follow interface module or other plugins.
	 * 4. Transport module is the last in the list.
	 * 5. As a special case, interface module can be the last, omitting
	 *    transport module. It creates a loopback. It needs to be treated
	 *    explicitly.
	 */
	pppoat_list_insert_tail(&p->pl_modules, mod);
}

void pppoat_pipeline_ready(struct pppoat_pipeline *p, bool ready)
{
	p->pl_is_ready = ready;
}

static int pipeline_packet_process(struct pppoat_pipeline *p,
				   struct pppoat_module   *mod,
				   struct pppoat_packet   *pkt)
{
	enum pppoat_packet_type  type = pkt->pkt_type;
	struct pppoat_module    *next;

	PPPOAT_ASSERT(type == PPPOAT_PACKET_SEND || type == PPPOAT_PACKET_RECV);

	/* XXX We don't support deferred packets atm. P_ERR() is for debug only. */
	if (!p->pl_is_ready)
		return P_ERR(-EAGAIN);

	next = type == PPPOAT_PACKET_SEND ?
				pppoat_list_next(&p->pl_modules, mod) :
				pppoat_list_prev(&p->pl_modules, mod);
	if (next == NULL)
		return P_ERR(-ENOENT);

	return pppoat_module_sendto(next, pkt);
}

int pppoat_pipeline_packet_send(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt)
{
	PPPOAT_ASSERT(pkt->pkt_type != PPPOAT_PACKET_RECV);

	if (pkt->pkt_type == PPPOAT_PACKET_UNKNOWN)
		pkt->pkt_type = PPPOAT_PACKET_SEND;

	/*
	 * Special case when the last module is interface and not transport.
	 * It implements a loopback.
	 */
	if (mod == pppoat_list_tail(&p->pl_modules) &&
	    pppoat_module_type(mod) == PPPOAT_MODULE_INTERFACE)
		pkt->pkt_type = PPPOAT_PACKET_RECV;

	return pipeline_packet_process(p, mod, pkt);
}

int pppoat_pipeline_packet_recv(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt)
{
	PPPOAT_ASSERT(pkt->pkt_type != PPPOAT_PACKET_SEND);

	if (pkt->pkt_type == PPPOAT_PACKET_UNKNOWN)
		pkt->pkt_type = PPPOAT_PACKET_RECV;

	return pipeline_packet_process(p, mod, pkt);
}
