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
#include "memory.h"
#include "module.h"
#include "packet.h"

#include <sys/types.h>
#include <signal.h>	/* kill */
#include <unistd.h>	/* read */

/*
 * TODO Add "file" bidirectional interface module which can transfer data
 * between file descriptors of two pppoat instances. The file can be
 * represented by a special (device, pipe) or regular file. In case of
 * regular file, transfer should be unidirectional.
 */

struct if_fd_ctx {
	int ifc_rd;
	int ifc_wr;
};

enum {
	IF_FD_MTU = 1500,
};

static bool if_fd_ctx_invariant(const struct if_fd_ctx *ctx)
{
	return ctx != NULL;
}

static int if_fd_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct if_fd_ctx *ctx;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	ctx->ifc_rd = -1;
	ctx->ifc_wr = -1;
	mod->m_userdata = ctx;

	return 0;
}

static void if_fd_fini(struct pppoat_module *mod)
{
	struct if_fd_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

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

static int if_fd_run(struct pppoat_module *mod)
{
	struct if_fd_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	return 0;
}

static int if_fd_stop(struct pppoat_module *mod)
{
	return 0;
}

static int if_fd_pkt_get(struct pppoat_module  *mod,
			 struct pppoat_packet **pkt)
{
	struct if_fd_ctx     *ctx = mod->m_userdata;
	struct pppoat_packet *pkt2;
	size_t                size;
	ssize_t               rlen;
	int                   fd;
	int                   rc;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));

	size = pppoat_module_mtu(mod);
	fd   = ctx->ifc_rd;
	pkt2 = pppoat_packet_get(mod->m_pkts, size);
	rc   = pkt2 == NULL ? P_ERR(-ENOMEM) : 0;

	rc = rc ?: pppoat_io_select_single_read(fd);
	if (rc == 0) {
		rlen = read(fd, pkt2->pkt_data, pkt2->pkt_size);
		if (rlen < 0) {
			rc = pppoat_io_error_is_recoverable(-errno) ?
			     -errno : P_ERR(-errno);
		}
		if (rlen == 0)
			rc = -ENOMSG;
		if (rlen > 0)
			pkt2->pkt_size = rlen;
	}
	if (rc != 0 && pkt2 != NULL)
		pppoat_packet_put(mod->m_pkts, pkt2);

	if (rc == 0) {
		pkt2->pkt_type = PPPOAT_PACKET_SEND;
		*pkt = pkt2;
	}
	return rc;
}

static int if_fd_pkt_process(struct pppoat_module  *mod,
			     struct pppoat_packet  *pkt_in,
			     struct pppoat_packet **pkt_out)
{
	struct if_fd_ctx *ctx = mod->m_userdata;
	int               rc;

	PPPOAT_ASSERT(if_fd_ctx_invariant(ctx));
	PPPOAT_ASSERT(pkt_in->pkt_type == PPPOAT_PACKET_RECV);

	rc = pppoat_io_write_sync(ctx->ifc_wr, pkt_in->pkt_data,
				  pkt_in->pkt_size);
	if (rc == 0)
		pppoat_packet_put(mod->m_pkts, pkt_in);

	*pkt_out = NULL;
	return rc;
}

static size_t if_fd_mtu(struct pppoat_module *mod)
{
	return IF_FD_MTU;
}

static struct pppoat_module_ops if_stdio_ops = {
	.mop_init        = &if_stdio_init,
	.mop_fini        = &if_stdio_fini,
	.mop_run         = &if_fd_run,
	.mop_stop        = &if_fd_stop,
	.mop_pkt_get     = &if_fd_pkt_get,
	.mop_pkt_process = &if_fd_pkt_process,
	.mop_mtu         = &if_fd_mtu,
};

struct pppoat_module_impl pppoat_module_if_stdio = {
	.mod_name  = "stdio",
	.mod_descr = "Standard in/out interface",
	.mod_type  = PPPOAT_MODULE_INTERFACE,
	.mod_ops   = &if_stdio_ops,
};
