/* modules/if_fd.c
 * PPP over Any Transport -- File descriptors interface module
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

#include "conf.h"
#include "io.h"
#include "misc.h"	/* container_of */
#include "memory.h"
#include "module.h"
#include "packet.h"
#include "pipeline.h"
#include "thread.h"

#include <sys/types.h>
#include <signal.h>	/* kill */
#include <unistd.h>	/* getpid, read */

/*
 * TODO Add "file" bidirectional interface module which can transfer data
 * between file descriptors of two pppoat instances. The file can be
 * represented by a special (device, pipe) or regular file. In case of
 * regular file, transfer should be unidirectional.
 */

struct if_fd_ctx {
	struct pppoat_thread  ifc_thread;
	struct pppoat_module *ifc_module;
	int                   ifc_rd;
	int                   ifc_wr;
};

enum {
	IF_FD_MTU = 1500,
};

static void if_fd_worker(struct pppoat_thread *thread);

static bool if_fd_ctx_invariant(const struct if_fd_ctx *ctx)
{
	return ctx != NULL;
}

static int if_fd_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct if_fd_ctx *ctx;
	int               rc;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	ctx->ifc_rd = -1;
	ctx->ifc_wr = -1;
	ctx->ifc_module = mod;
	mod->m_userdata = ctx;

	rc = pppoat_thread_init(&ctx->ifc_thread, &if_fd_worker);

	return rc;
}

static void if_fd_fini(struct pppoat_module *mod)
{
	struct if_fd_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	pppoat_thread_fini(&ctx->ifc_thread);
	pppoat_free(ctx);
}

static int if_stdio_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct if_fd_ctx *ctx;
	int               rc;

	rc = if_fd_init(mod, conf);
	if (rc == 0) {
		ctx = mod->m_userdata;
		PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));
		ctx->ifc_rd = STDIN_FILENO;
		ctx->ifc_wr = STDOUT_FILENO;
	}
	return rc;
}

static void if_stdio_fini(struct pppoat_module *mod)
{
	if_fd_fini(mod);
}

static void if_fd_worker(struct pppoat_thread *thread)
{
	struct if_fd_ctx       *ctx =
			container_of(thread, struct if_fd_ctx, ifc_thread);
	struct pppoat_packets  *pkts;
	struct pppoat_pipeline *pipeline;
	struct pppoat_packet   *pkt;
	size_t                  size;
	ssize_t                 rlen;
	int                     rc = 0;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	pipeline = ctx->ifc_module->m_pipeline;
	pkts = ctx->ifc_module->m_pkts;
	size = pppoat_module_mtu(ctx->ifc_module);
	pkt  = pppoat_packet_get(pkts, size);
	while (rc == 0 && pkt != NULL) {
		rc = pppoat_io_select_single_read(ctx->ifc_rd);
		if (rc != 0)
			break;

		rlen = read(ctx->ifc_rd, pkt->pkt_data, pkt->pkt_size);
		if (rlen == 0) {
			pppoat_debug("fd", "EOF reached, exiting...");
			rc = kill(getpid(), SIGINT);
			PPPOAT_ASSERT(rc == 0); /* XXX */
			/* TODO Need to make remote pppoat quit as well. */
		}
		if (rlen < 0 && !pppoat_io_error_is_recoverable(-errno))
			rc = P_ERR(-errno);
		if (rlen > 0) {
			pkt->pkt_size = rlen;
			rc = pppoat_pipeline_packet_send(pipeline,
							 ctx->ifc_module, pkt);
			if (rc != 0)
				pppoat_packet_put(pkts, pkt);
			pkt = pppoat_packet_get(pkts, size);
		}
	}
	if (pkt != NULL)
		pppoat_packet_put(pkts, pkt);
	pppoat_debug("fd", "Worker thread finished. rc=%d pkt=%p", rc, pkt);
}

static int if_fd_run(struct pppoat_module *mod)
{
	struct if_fd_ctx *ctx = mod->m_userdata;
	int               rc;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	rc = pppoat_thread_start(&ctx->ifc_thread);

	return rc;
}

static int if_fd_stop(struct pppoat_module *mod)
{
	/* TODO */
	return 0;
}

static int if_fd_recv(struct pppoat_module *mod, struct pppoat_packet *pkt)
{
	struct if_fd_ctx *ctx = mod->m_userdata;
	int               rc;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	rc = pppoat_io_write_sync(ctx->ifc_wr, pkt->pkt_data, pkt->pkt_size);
	if (rc == 0)
		pppoat_packet_put(mod->m_pkts, pkt);

	return rc;
}

static size_t if_fd_mtu(struct pppoat_module *mod)
{
	return IF_FD_MTU;
}

static struct pppoat_module_ops if_stdio_ops = {
	.mop_init = &if_stdio_init,
	.mop_fini = &if_stdio_fini,
	.mop_run  = &if_fd_run,
	.mop_stop = &if_fd_stop,
	.mop_recv = &if_fd_recv,
	.mop_mtu  = &if_fd_mtu,
};

struct pppoat_module_impl pppoat_module_if_stdio = {
	.mod_name  = "stdio",
	.mod_descr = "Standard in/out interface",
	.mod_type  = PPPOAT_MODULE_INTERFACE,
	.mod_ops   = &if_stdio_ops,
};
