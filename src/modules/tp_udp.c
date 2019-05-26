/* modules/tp_udp.c
 * PPP over Any Transport -- UDP transport module
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
#include "module.h"
#include "packet.h"
#include "pipeline.h"
#include "thread.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define UDP_CONF_PORT  "udp.port"
#define UDP_CONF_SPORT "udp.sport"
#define UDP_CONF_DPORT "udp.dport"
#define UDP_CONF_HOST  "udp.host"

struct tp_udp_ctx {
	struct pppoat_module *uc_module;
	struct addrinfo      *uc_ainfo;
	struct pppoat_thread  uc_thread;
	int                   uc_sock;
	char                 *uc_dhost;
	unsigned short        uc_sport;
	unsigned short        uc_dport;
	uint32_t              uc_magic;
};

enum {
	TP_UDP_MTU = 1500,
};

static void tp_udp_worker(struct pppoat_thread *thread);

static bool tp_udp_ctx_invariant(struct tp_udp_ctx *ctx)
{
	return ctx != NULL;
}

static int tp_udp_ainfo_get(struct addrinfo **ainfo,
			    const char       *host,
			    unsigned short    port)
{
	struct addrinfo hints;
	char            service[6];
	int             rc;

	memset(&hints, 0, sizeof(hints));
	hints.ai_flags    = host == NULL ? AI_PASSIVE : 0;
#ifdef AI_ADDRCONFIG
	hints.ai_flags   |= AI_ADDRCONFIG;
#endif /* AI_ADDRCONFIG */
#ifdef __APPLE__
	hints.ai_family   = AF_INET;
#else /* __APPLE__ */
	hints.ai_family   = AF_UNSPEC;
#endif /* __APPLE__ */
	hints.ai_protocol = IPPROTO_UDP;
	hints.ai_socktype = SOCK_DGRAM;

	snprintf(service, sizeof(service), "%u", port);
	rc = getaddrinfo(host, service, &hints, ainfo);
	if (rc != 0) {
		pppoat_error("udp", "getaddrinfo rc=%d: %s",
			     rc, gai_strerror(rc));
		rc = P_ERR(-ENOPROTOOPT);
		*ainfo = NULL;
	}
	return rc;
}

static void tp_udp_ainfo_put(struct addrinfo *ainfo)
{
	freeaddrinfo(ainfo);
}

static int tp_udp_sock_new(unsigned short port, int *sock)
{
	struct addrinfo *ainfo;
	int              rc;

	rc = tp_udp_ainfo_get(&ainfo, NULL, port);
	if (rc == 0) {
		*sock = socket(ainfo->ai_family, ainfo->ai_socktype,
			       ainfo->ai_protocol);
		rc = *sock < 0 ? P_ERR(-errno) : 0;
	}
	if (rc == 0) {
		rc = bind(*sock, ainfo->ai_addr, ainfo->ai_addrlen);
		rc = rc != 0 ? P_ERR(-errno) : 0;
		if (rc != 0)
			(void)close(*sock);
	}
	if (ainfo != NULL)
		tp_udp_ainfo_put(ainfo);

	return rc;
}

static int tp_udp_conf_parse(struct tp_udp_ctx *ctx, struct pppoat_conf *conf)
{
	long port;
	int  rc;

	ctx->uc_sport = 0;
	ctx->uc_dport = 0;

	rc = pppoat_conf_find_long(conf, UDP_CONF_PORT, &port);
	if (rc == 0) {
		ctx->uc_sport = (unsigned short)port;
		ctx->uc_dport = (unsigned short)port;
	}
	rc = pppoat_conf_find_long(conf, UDP_CONF_SPORT, &port);
	if (rc == 0)
		ctx->uc_sport = (unsigned short)port;
	rc = pppoat_conf_find_long(conf, UDP_CONF_DPORT, &port);
	if (rc == 0)
		ctx->uc_dport = (unsigned short)port;

	PPPOAT_ASSERT(ctx->uc_sport != 0 && ctx->uc_dport != 0); /* XXX */

	rc = pppoat_conf_find_string_alloc(conf, UDP_CONF_HOST, &ctx->uc_dhost);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	return rc;
}

static void tp_udp_conf_fini(struct tp_udp_ctx *ctx)
{
	pppoat_free(ctx->uc_dhost);
}

static int tp_udp_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct tp_udp_ctx *ctx;
	int                rc;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);
	rc = tp_udp_conf_parse(ctx, conf);
	if (rc != 0)
		goto err_ctx_free;

	rc = tp_udp_ainfo_get(&ctx->uc_ainfo, ctx->uc_dhost, ctx->uc_dport);
	if (rc != 0)
		goto err_conf_fini;

	rc = tp_udp_sock_new(ctx->uc_sport, &ctx->uc_sock);
	if (rc != 0)
		goto err_ainfo_put;

	(void)pppoat_io_fd_blocking_set(ctx->uc_sock, false);

	rc = pppoat_thread_init(&ctx->uc_thread, &tp_udp_worker);
	if (rc != 0)
		goto err_sock_close;

	ctx->uc_module = mod;
	ctx->uc_thread.t_userdata = ctx;
	mod->m_userdata = ctx;

	return 0;

err_sock_close:
	(void)close(ctx->uc_sock);
err_ainfo_put:
	tp_udp_ainfo_put(ctx->uc_ainfo);
err_conf_fini:
	tp_udp_conf_fini(ctx);
err_ctx_free:
	pppoat_free(ctx);
	return rc;
}

static void tp_udp_fini(struct pppoat_module *mod)
{
	struct tp_udp_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(tp_udp_ctx_invariant(ctx));

	(void)close(ctx->uc_sock);
	tp_udp_ainfo_put(ctx->uc_ainfo);
	tp_udp_conf_fini(ctx);
	pppoat_free(ctx);
}

static void tp_udp_worker(struct pppoat_thread *thread)
{
	struct tp_udp_ctx      *ctx = thread->t_userdata;
	struct pppoat_packets  *pkts;
	struct pppoat_pipeline *pipeline;
	struct pppoat_packet   *pkt;
	void                   *buf;
	size_t                  size;
	ssize_t                 rlen;
	int                     sock;
	int                     rc = 0;

	PPPOAT_ASSERT(tp_udp_ctx_invariant(ctx));

	pipeline = ctx->uc_module->m_pipeline;
	pkts = ctx->uc_module->m_pkts;
	sock = ctx->uc_sock;
	size = TP_UDP_MTU;
	while (rc == 0) {
		rc = pppoat_io_select_single_read(sock);
		if (rc == 0) {
			buf = pppoat_alloc(size);
			PPPOAT_ASSERT(buf != NULL); /* XXX */
			/* XXX use recvfrom() */
			rlen = recv(sock, buf, size, 0);
			if (rlen < 0 && !pppoat_io_error_is_recoverable(-errno))
				rc = P_ERR(-errno);
			if (rlen > 0) {
				pkt = pppoat_packet_get(pkts);
				PPPOAT_ASSERT(pkt != NULL); /* XXX */
				pkt->pkt_data = buf;
				pkt->pkt_size = rlen;
				pkt->pkt_ops = &pppoat_packet_ops_std;
				pppoat_debug("udp", "Recv pkt size=%zu", pkt->pkt_size);
				rc = pppoat_pipeline_packet_recv(pipeline,
							ctx->uc_module, pkt);
				if (rc != 0)
					pppoat_packet_put(pkts, pkt);
			} else
				pppoat_free(buf);
		}
	}
	pppoat_debug("udp", "Worker thread finished.");
}

static int tp_udp_run(struct pppoat_module *mod)
{
	struct tp_udp_ctx *ctx = mod->m_userdata;
	int                rc;

	PPPOAT_ASSERT(tp_udp_ctx_invariant(ctx));

	rc = pppoat_thread_start(&ctx->uc_thread);

	return rc;
}

static int tp_udp_stop(struct pppoat_module *mod)
{
	/* TODO */
	return 0;
}

static int tp_udp_buf_send(int              sock,
			   struct addrinfo *ainfo,
			   unsigned char   *buf,
			   size_t           len)
{
	ssize_t slen = 0;
	int     rc   = 0;

	do {
		slen = sendto(sock, buf, len, 0, ainfo->ai_addr,
			      ainfo->ai_addrlen);
		if (slen < 0 && errno == EINTR)
			continue;
		if (slen < 0 && !pppoat_io_error_is_recoverable(-errno))
			rc = P_ERR(-errno);
		if (slen < 0 && pppoat_io_error_is_recoverable(-errno))
			rc = pppoat_io_select_single_write(sock);
		if (slen > 0) {
			buf += slen;
			len -= slen;
		}
	} while (rc == 0 && len > 0);

	return rc;
}

static int tp_udp_recv(struct pppoat_module *mod, struct pppoat_packet *pkt)
{
	struct tp_udp_ctx *ctx = mod->m_userdata;
	int                rc;

	PPPOAT_ASSERT(tp_udp_ctx_invariant(ctx));
	PPPOAT_ASSERT(pkt->pkt_type == PPPOAT_PACKET_SEND);

	pppoat_debug("udp", "Send pkt size=%zu", pkt->pkt_size);

	rc = tp_udp_buf_send(ctx->uc_sock, ctx->uc_ainfo, pkt->pkt_data,
			     pkt->pkt_size);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	pppoat_packet_put(mod->m_pkts, pkt);

	return 0;
}

static size_t tp_udp_mtu(struct pppoat_module *mod)
{
	return TP_UDP_MTU;
}

static struct pppoat_module_ops tp_udp_ops = {
	.mop_init = &tp_udp_init,
	.mop_fini = &tp_udp_fini,
	.mop_run  = &tp_udp_run,
	.mop_stop = &tp_udp_stop,
	.mop_recv = &tp_udp_recv,
	.mop_mtu  = &tp_udp_mtu,
};

struct pppoat_module_impl pppoat_module_tp_udp = {
	.mod_name  = "udp",
	.mod_descr = "UDP transport",
	.mod_type  = PPPOAT_MODULE_TRANSPORT,
	.mod_ops   = &tp_udp_ops,
};
