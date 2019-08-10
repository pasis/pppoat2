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

static void pipeline_loop_thread(struct pppoat_thread *thread);

static struct pppoat_list_descr pipeline_descr =
	PPPOAT_LIST_DESCR("Pipeline", struct pppoat_module, m_link, m_magic,
			  PPPOAT_PIPELINE_MAGIC);

int pppoat_pipeline_init(struct pppoat_pipeline *p)
{
	int rc;

	p->pl_running = false;
	pppoat_list_init(&p->pl_modules, &pipeline_descr);
	rc = pppoat_thread_init(&p->pl_thread_send, &pipeline_loop_thread)
	  ?: pppoat_thread_init(&p->pl_thread_recv, &pipeline_loop_thread);
	p->pl_thread_send.t_userdata = p;
	p->pl_thread_recv.t_userdata = p;

	return rc;
}

static void pipeline_flush(struct pppoat_pipeline *p)
{
	while (!pppoat_list_is_empty(&p->pl_modules))
		(void)pppoat_list_pop(&p->pl_modules);
}

void pppoat_pipeline_fini(struct pppoat_pipeline *p)
{
	PPPOAT_ASSERT(!p->pl_running);

	pppoat_thread_fini(&p->pl_thread_send);
	pppoat_thread_fini(&p->pl_thread_recv);
	pipeline_flush(p);
	pppoat_list_fini(&p->pl_modules);
}

int pppoat_pipeline_start(struct pppoat_pipeline *p)
{
	int rc;

	rc = pppoat_thread_start(&p->pl_thread_send)
	  ?: pppoat_thread_start(&p->pl_thread_recv);
	p->pl_running = rc == 0;

	return rc;
}

void pppoat_pipeline_stop(struct pppoat_pipeline *p)
{
	/* XXX TODO Stop condition.
	int rc;

	rc = pppoat_thread_join(&p->pl_thread_send)
	  ?: pppoat_thread_join(&p->pl_thread_recv);
	PPPOAT_ASSERT(rc == 0);
	*/
	p->pl_running = false;
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

static int pipeline_module_process(struct pppoat_pipeline *p,
				   struct pppoat_module   *mod)
{
	struct pppoat_packet *pkt;
	struct pppoat_packet *pkt_next;
	int                   rc;

	/*
	 * TODO Handle loopback when interface module is used instead of
	 * transport.
	 */

	rc = pppoat_module_pkt_get(mod, &pkt);
	while (rc == 0 && pkt != NULL) {
		if (pkt->pkt_type == PPPOAT_PACKET_SEND)
			mod = pppoat_list_next(&p->pl_modules, mod);
		else
			mod = pppoat_list_prev(&p->pl_modules, mod);

		PPPOAT_ASSERT(pkt->pkt_type == PPPOAT_PACKET_SEND ||
			      pkt->pkt_type == PPPOAT_PACKET_RECV);
		PPPOAT_ASSERT(mod != NULL);

		rc = pppoat_module_pkt_process(mod, pkt, &pkt_next);
		if (rc != 0)
			pppoat_packet_put(mod->m_pkts, pkt);
		pkt = pkt_next;
	}
	return rc;
}

static void pipeline_loop_thread(struct pppoat_thread *thread)
{
	struct pppoat_pipeline *p = thread->t_userdata;
	struct pppoat_module   *mod;
	int                     rc;

	/* Cache interface or transport module. */
	if (thread == &p->pl_thread_send)
		mod = pppoat_list_head(&p->pl_modules);
	else {
		PPPOAT_ASSERT(thread == &p->pl_thread_recv);
		mod = pppoat_list_tail(&p->pl_modules);
	}

	while (true) {
		rc = pipeline_module_process(p, mod);
		if (rc != 0)
			pppoat_error("pipeline", "Error while processing a "
						 "packet (rc=%d)", rc);
	}
}
