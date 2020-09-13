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
#include "misc.h"
#include "module.h"
#include "packet.h"
#include "pipeline.h"

static void pipeline_blocking_thread(struct pppoat_thread *thread);
static void pipeline_loop_thread(struct pppoat_thread *thread);

static struct pppoat_list_descr pipeline_descr =
	PPPOAT_LIST_DESCR("Pipeline", struct pppoat_module, m_link, m_magic,
			  PPPOAT_PIPELINE_MAGIC);

int pppoat_pipeline_init(struct pppoat_pipeline *p)
{
	int rc = 0;

	p->pl_running = false;
	pppoat_list_init(&p->pl_modules, &pipeline_descr);

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

	pipeline_flush(p);
	pppoat_list_fini(&p->pl_modules);
}

static int pipeline_start_if_blocking(struct pppoat_pipeline *p,
				      struct pppoat_module   *mod,
				      struct pppoat_thread   *thread)
{
	int rc = 0;

	if (pppoat_module_is_blocking(mod)) {
		rc = pppoat_thread_init(thread, &pipeline_blocking_thread);
		if (rc == 0) {
			thread->t_userdata = p;
			rc = pppoat_thread_start(thread);
			if (rc != 0)
				pppoat_thread_fini(thread);
		}
	}
	return rc;
}

int pppoat_pipeline_start(struct pppoat_pipeline *p)
{
	int rc;

	PPPOAT_ASSERT(p->pl_modules_nr > 1);

	p->pl_running = true;

	/*
	 * Start a thread per blocking module. Blocking modules are simplified
	 * modules without event handling. Such a module reads/writes data in
	 * blocking manner and doesn't support non-blocking polling.
	 *
	 * XXX If an error happens, already started threads won't be cleaned up.
	 */

	rc = pipeline_start_if_blocking(p, pppoat_list_head(&p->pl_modules),
					&p->pl_thread_blk1)
	  ?: pipeline_start_if_blocking(p, pppoat_list_tail(&p->pl_modules),
					&p->pl_thread_blk2);
	if (rc != 0)
		goto quit;

	/*
	 * Start a main loop if we have at least one non-blocking module.
	 */

	if (!pppoat_module_is_blocking(pppoat_list_head(&p->pl_modules)) ||
	    !pppoat_module_is_blocking(pppoat_list_tail(&p->pl_modules)) ||
	    p->pl_modules_nr > 2) {
		rc = pppoat_thread_init(&p->pl_thread, &pipeline_loop_thread);
		if (rc == 0) {
			p->pl_thread.t_userdata = p;
			rc = pppoat_thread_start(&p->pl_thread);
			if (rc != 0)
				pppoat_thread_fini(&p->pl_thread);
		}
	}

quit:
	p->pl_running = rc == 0;

	return rc;
}

void pppoat_pipeline_stop(struct pppoat_pipeline *p)
{
	int rc;

	p->pl_running = false;

	if (!pppoat_module_is_blocking(pppoat_list_head(&p->pl_modules)) ||
	    !pppoat_module_is_blocking(pppoat_list_tail(&p->pl_modules)) ||
	    p->pl_modules_nr > 2) {
		rc = pppoat_thread_join(&p->pl_thread);
		PPPOAT_ASSERT(rc == 0);
		pppoat_thread_fini(&p->pl_thread);
	}
	if (pppoat_module_is_blocking(pppoat_list_tail(&p->pl_modules))) {
		(void)pppoat_thread_cancel(&p->pl_thread_blk2);
		rc = pppoat_thread_join(&p->pl_thread_blk2);
		PPPOAT_ASSERT(rc == 0);
		pppoat_thread_fini(&p->pl_thread_blk2);
	}
	if (pppoat_module_is_blocking(pppoat_list_head(&p->pl_modules))) {
		(void)pppoat_thread_cancel(&p->pl_thread_blk1);
		rc = pppoat_thread_join(&p->pl_thread_blk1);
		PPPOAT_ASSERT(rc == 0);
		pppoat_thread_fini(&p->pl_thread_blk1);
	}
}

static bool pipeline_modules_list_invariant(struct pppoat_pipeline *p)
{
	struct pppoat_module *mod = pppoat_list_head(&p->pl_modules);
	bool                  result;

	/*
	 * Check that the 1st module is not a plugin and all modules in the
	 * middle are plugins. The last module may be plugin, because the list
	 * may be unfinished.
	 */

	if (mod == NULL)
		return true;
	if (pppoat_module_type(mod) == PPPOAT_MODULE_PLUGIN)
		return false;
	mod = pppoat_list_next(&p->pl_modules, mod);
	while (mod != NULL) {
		result = pppoat_module_type(mod) == PPPOAT_MODULE_PLUGIN;
		mod = pppoat_list_next(&p->pl_modules, mod);
		if (!result && mod != NULL)
			return false;
	}
	return true;
}

void pppoat_pipeline_add_module(struct pppoat_pipeline *p,
				struct pppoat_module   *mod)
{
	/*
	 * Restrictions on the modules list:
	 * 1. The 1st and the last modules must be interface or transport.
	 *    a. Special case when the both edge modules are interface is
	 *       a loopback.
	 *    b. Special case when the both edge modules are transport is
	 *       a gateway.
	 * 2. Interface and transport modules cannot be in the middle of the
	 *    list. Only plugins allowed in the middle.
	 */
	PPPOAT_ASSERT(pipeline_modules_list_invariant(p));

	/* Check for loopback and gateway modes. */
	if ((pppoat_list_is_empty(&p->pl_modules) &&
	     pppoat_module_type(mod) == PPPOAT_MODULE_TRANSPORT) ||
	    (!pppoat_list_is_empty(&p->pl_modules) &&
	     pppoat_module_type(mod) == PPPOAT_MODULE_INTERFACE)) {
		mod->m_invert = true;
	}

	pppoat_list_insert_tail(&p->pl_modules, mod);
	++p->pl_modules_nr;
}

static int pipeline_module_process(struct pppoat_pipeline *p,
				   struct pppoat_module   *mod)
{
	struct pppoat_packet *pkt;
	struct pppoat_packet *pkt_next;
	int                   rc;

	rc = pppoat_module_process(mod, NULL, &pkt);
	while (rc == 0 && pkt != NULL) {
		if (pkt->pkt_type == PPPOAT_PACKET_SEND)
			mod = pppoat_list_next(&p->pl_modules, mod);
		else
			mod = pppoat_list_prev(&p->pl_modules, mod);

		PPPOAT_ASSERT(pkt->pkt_type == PPPOAT_PACKET_SEND ||
			      pkt->pkt_type == PPPOAT_PACKET_RECV);
		PPPOAT_ASSERT(mod != NULL);

		rc = pppoat_module_process(mod, pkt, &pkt_next);
		if (rc != 0)
			pppoat_packet_put(mod->m_pkts, pkt);
		pkt = pkt_next;
	}
	if (rc != 0) {
		pppoat_error("pipeline", "Error during processing module '%s' "
			     "(rc=%d)", pppoat_module_name(mod), rc);
	}
	return rc;
}

static void pipeline_blocking_thread(struct pppoat_thread *thread)
{
	struct pppoat_pipeline *p = thread->t_userdata;
	struct pppoat_module   *mod = NULL;

	PPPOAT_ASSERT(thread == &p->pl_thread_blk1 ||
		      thread == &p->pl_thread_blk2);
	if (thread == &p->pl_thread_blk1)
		mod = pppoat_list_head(&p->pl_modules);
	if (thread == &p->pl_thread_blk2)
		mod = pppoat_list_tail(&p->pl_modules);

	while (p->pl_running) {
		pipeline_module_process(p, mod);
	}
}

static void pipeline_loop_thread(struct pppoat_thread *thread)
{
	struct pppoat_pipeline *p =
			container_of(thread, struct pppoat_pipeline, pl_thread);
	struct pppoat_module   *mod;

	while (p->pl_running) {
		mod = pppoat_list_head(&p->pl_modules);
		while (mod != NULL) {
			if (!pppoat_module_is_blocking(mod))
				pipeline_module_process(p, mod);
			mod = pppoat_list_next(&p->pl_modules, mod);
		}
	}
}
