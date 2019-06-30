/* modules/if_pppd.c
 * PPP over Any Transport -- PPP interface module
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
#include "magic.h"
#include "memory.h"
#include "misc.h"
#include "module.h"
#include "packet.h"
#include "pipeline.h"
#include "thread.h"

#include <errno.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <signal.h>	/* kill */
#include <stdbool.h>
#include <unistd.h>	/* access, fork, pipe, dup2, exit, ... */

struct if_pppd_ctx {
	struct pppoat_thread  ipc_thread;
	struct pppoat_module *ipc_module;
	const char           *ipc_pppd_path;
	pid_t                 ipc_pppd_pid;
	int                   ipc_rd;
	int                   ipc_wr;
	char                 *ipc_ip;
	uint32_t              ipc_magic;
};

#define PPPD_CONF_IP   "pppd.ip"
#define PPPD_CONF_PATH "pppd.path"

enum {
	IF_PPPD_MTU = 1500,
};

static const char *pppd_paths[] = {
	"/sbin/pppd",
	"/usr/sbin/pppd",
	"/usr/local/sbin/pppd",
	"/usr/bin/pppd",
	"/usr/local/bin/pppd",
};

static void if_pppd_worker(struct pppoat_thread *thread);

static const char *if_pppd_path(void)
{
	int i;
	int rc;

	/* TODO Make conf option 'pppd.path'. */

	for (i = 0; i < ARRAY_SIZE(pppd_paths); ++i) {
		rc = access(pppd_paths[i], X_OK);
		rc = rc == 0 ? 0 : -errno;
		if (rc == 0)
			return pppd_paths[i];
		if (rc != -ENOENT)
			pppoat_info("pppd",
				    "%s exists, but not executable (rc=%d)",
				    pppd_paths[i], rc);
	}
	return NULL;
}

static bool if_pppd_ctx_invariant(const struct if_pppd_ctx *ctx)
{
	return ctx != NULL &&
	       ctx->ipc_magic == PPPOAT_MODULE_IF_PPPD_MAGIC;
}

static int if_pppd_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct if_pppd_ctx *ctx;
	int                 rc;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	ctx->ipc_pppd_path = if_pppd_path();
	if (ctx->ipc_pppd_path == NULL) {
		rc = P_ERR(-ENOENT);
		goto err;
	}
	
	ctx->ipc_ip = NULL;
	rc = pppoat_conf_find_string_alloc(conf, PPPD_CONF_IP, &ctx->ipc_ip);
	if (rc != 0 && rc != -ENOENT)
		goto err;

	rc = pppoat_thread_init(&ctx->ipc_thread, &if_pppd_worker);
	if (rc != 0)
		goto err_free_ip;

	ctx->ipc_module = mod;
	ctx->ipc_magic = PPPOAT_MODULE_IF_PPPD_MAGIC;
	mod->m_userdata = ctx;

	return 0;

err_free_ip:
	pppoat_free(ctx->ipc_ip);
err:
	pppoat_free(ctx);
	return rc;
}

static void if_pppd_fini(struct pppoat_module *mod)
{
	struct if_pppd_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(if_pppd_ctx_invariant(ctx));

	pppoat_thread_fini(&ctx->ipc_thread);
	pppoat_free(ctx->ipc_ip);
	pppoat_free(ctx);
	mod->m_userdata = NULL;
}

static void if_pppd_worker(struct pppoat_thread *thread)
{
	struct if_pppd_ctx     *ctx =
			container_of(thread, struct if_pppd_ctx, ipc_thread);
	struct pppoat_packets  *pkts;
	struct pppoat_pipeline *pipeline;
	struct pppoat_packet   *pkt;
	size_t                  size;
	ssize_t                 rlen;
	int                     fd;
	int                     rc = 0;

	PPPOAT_ASSERT(if_pppd_ctx_invariant(ctx));

	pipeline = ctx->ipc_module->m_pipeline;
	pkts = ctx->ipc_module->m_pkts;
	fd   = ctx->ipc_rd;
	size = IF_PPPD_MTU;
	pkt  = pppoat_packet_get(pkts, size);
	while (rc == 0 && pkt != NULL) {
		rc = pppoat_io_select_single_read(fd);
		if (rc != 0)
			break;

		rlen = read(fd, pkt->pkt_data, pkt->pkt_size);
		if (rlen < 0 && !pppoat_io_error_is_recoverable(-errno))
			rc = P_ERR(-errno);
		if (rlen > 0) {
			pkt->pkt_size = rlen;
			rc = pppoat_pipeline_packet_send(pipeline,
							 ctx->ipc_module, pkt);
			if (rc != 0)
				pppoat_packet_put(pkts, pkt);
			pkt  = pppoat_packet_get(pkts, size);
		}
	}
	if (pkt != NULL)
		pppoat_packet_put(pkts, pkt);
	pppoat_debug("pppd", "Worker thread finished. rc=%d pkt=%p", rc, pkt);
}

static int if_pppd_run(struct pppoat_module *mod)
{
	struct if_pppd_ctx *ctx = mod->m_userdata;
	pid_t               pid;
	int                 pipe_rd[2];
	int                 pipe_wr[2];
	int                 rc;

	PPPOAT_ASSERT(if_pppd_ctx_invariant(ctx));

	rc = pipe(pipe_rd);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = pipe(pipe_wr);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	pid = fork();
	if (pid == -1)
		return P_ERR(-errno);

	if (pid == 0) {
		rc = dup2(pipe_wr[0], STDIN_FILENO);
		PPPOAT_ASSERT(rc != -1); /* XXX */
		rc = dup2(pipe_rd[1], STDOUT_FILENO);
		PPPOAT_ASSERT(rc != -1); /* XXX */
		close(pipe_rd[0]);
		close(pipe_rd[1]);
		close(pipe_wr[0]);
		close(pipe_wr[1]);

		pppoat_debug("pppd", "%s nodetach noauth notty passive %s",
			     ctx->ipc_pppd_path,
			     ctx->ipc_ip == NULL ? "" : ctx->ipc_ip);
		pppoat_log_flush();
		rc = execl(ctx->ipc_pppd_path, ctx->ipc_pppd_path, "nodetach",
			   "noauth", "notty", "passive", ctx->ipc_ip, NULL);
		pppoat_error("pppd", "Failed to execute pppd, rc=%d errno=%d",
			     rc, errno);
		exit(1);
	}

	close(pipe_rd[1]);
	close(pipe_wr[0]);

	PPPOAT_ASSERT(pid > 0);
	ctx->ipc_pppd_pid = pid;
	ctx->ipc_rd       = pipe_rd[0];
	ctx->ipc_wr       = pipe_wr[1];

	(void)pppoat_io_fd_blocking_set(ctx->ipc_rd, false);
	(void)pppoat_io_fd_blocking_set(ctx->ipc_wr, false);

	rc = pppoat_thread_start(&ctx->ipc_thread);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	return 0;
}

static int if_pppd_stop(struct pppoat_module *mod)
{
	struct if_pppd_ctx *ctx = mod->m_userdata;
	pid_t               pid;
	int                 rc;

	PPPOAT_ASSERT(if_pppd_ctx_invariant(ctx));

	/* TODO Stop and fini the thread. */

	rc = kill(ctx->ipc_pppd_pid, SIGTERM);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	do {
		pid = waitpid(ctx->ipc_pppd_pid, NULL, 0);
	} while (pid < 0 && errno == EINTR);

	PPPOAT_ASSERT(pid == ctx->ipc_pppd_pid); /* XXX */

	close(ctx->ipc_rd);
	close(ctx->ipc_wr);

	return 0;
}

static int if_pppd_recv(struct pppoat_module *mod, struct pppoat_packet *pkt)
{
	struct if_pppd_ctx *ctx = mod->m_userdata;
	int                 rc;

	PPPOAT_ASSERT(if_pppd_ctx_invariant(ctx));

	rc = pppoat_io_write_sync(ctx->ipc_wr, pkt->pkt_data, pkt->pkt_size);
	if (rc == 0)
		pppoat_packet_put(mod->m_pkts, pkt);

	return rc;
}

static size_t if_pppd_mtu(struct pppoat_module *mod)
{
	return IF_PPPD_MTU;
}

static struct pppoat_module_ops if_pppd_ops = {
	.mop_init = &if_pppd_init,
	.mop_fini = &if_pppd_fini,
	.mop_run  = &if_pppd_run,
	.mop_stop = &if_pppd_stop,
	.mop_recv = &if_pppd_recv,
	.mop_mtu  = &if_pppd_mtu,
};

struct pppoat_module_impl pppoat_module_if_pppd = {
	.mod_name  = "pppd",
	.mod_descr = "PPP interface via pppd",
	.mod_type  = PPPOAT_MODULE_INTERFACE,
	.mod_ops   = &if_pppd_ops,
};
