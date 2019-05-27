/* modules/if_tun.c
 * PPP over Any Transport -- TUN/TAP interface module
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

#include <string.h>		/* strlen */
#include <unistd.h>		/* open, close, read */

enum {
	IF_TUN_MTU = 1500,
	IF_TAP_MTU = 1500,
};

enum if_tuntap_type {
	PPPOAT_IF_TUN = 1,
	PPPOAT_IF_TAP,
};

struct if_tuntap_ctx {
	enum if_tuntap_type   itc_type;
	struct pppoat_thread  itc_thread;
	struct pppoat_module *itc_module;
	char                 *itc_ifname;
	int                   itc_fd;
};

static int if_tuntap_fd_init(struct if_tuntap_ctx *ctx,
			     struct pppoat_conf   *conf,
			     enum if_tuntap_type   type);
static void if_tuntap_fd_fini(struct if_tuntap_ctx *ctx);
static void if_tuntap_worker(struct pppoat_thread *thread);
static void if_tun_compat_layer(struct if_tuntap_ctx *ctx,
				struct pppoat_packet *pkt,
				bool                  send);

static bool if_tuntap_ctx_invariant(const struct if_tuntap_ctx *ctx)
{
	return ctx != NULL;
}


static int if_tuntap_init(struct pppoat_module *mod,
			  struct pppoat_conf   *conf,
			  enum if_tuntap_type   type)
{
	struct if_tuntap_ctx *ctx;
	int                   rc;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	ctx->itc_type   = type;
	ctx->itc_module = mod;
	mod->m_userdata = ctx;

	rc = if_tuntap_fd_init(ctx, conf, type);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	rc = pppoat_thread_init(&ctx->itc_thread, &if_tuntap_worker);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	ctx->itc_thread.t_userdata = ctx;

	if (rc == 0)
		pppoat_debug("tun", "Created interface %s", ctx->itc_ifname);

	return 0;
}

static int if_tun_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	return if_tuntap_init(mod, conf, PPPOAT_IF_TUN);
}

static int if_tap_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	return if_tuntap_init(mod, conf, PPPOAT_IF_TAP);
}

static void if_tuntap_fini(struct pppoat_module *mod)
{
	struct if_tuntap_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(if_tuntap_ctx_invariant(ctx));

	pppoat_thread_fini(&ctx->itc_thread);
	if_tuntap_fd_fini(ctx);
	pppoat_free(ctx);
}

static void if_tuntap_worker(struct pppoat_thread *thread)
{
	struct if_tuntap_ctx   *ctx = thread->t_userdata;
	struct pppoat_packets  *pkts;
	struct pppoat_pipeline *pipeline;
	struct pppoat_packet   *pkt;
	size_t                  size;
	ssize_t                 rlen;
	int                     fd;
	int                     rc = 0;

	PPPOAT_ASSERT(if_tuntap_ctx_invariant(ctx));

	pipeline = ctx->itc_module->m_pipeline;
	pkts = ctx->itc_module->m_pkts;
	fd   = ctx->itc_fd;
	size = pppoat_module_mtu(ctx->itc_module);
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
			if_tun_compat_layer(ctx, pkt, true);
			rc = pppoat_pipeline_packet_send(pipeline,
							 ctx->itc_module, pkt);
			if (rc != 0)
				pppoat_packet_put(pkts, pkt);
			pkt = pppoat_packet_get(pkts, size);
		}
	}
	if (pkt != NULL)
		pppoat_packet_put(pkts, pkt);
	pppoat_debug("tun", "Worker thread finished. rc=%d pkt=%p", rc, pkt);
}

static int if_tuntap_run(struct pppoat_module *mod)
{
	struct if_tuntap_ctx *ctx = mod->m_userdata;
	int                   rc;

	PPPOAT_ASSERT(if_tuntap_ctx_invariant(ctx));

	rc = pppoat_thread_start(&ctx->itc_thread);

	return rc;
}

static int if_tuntap_stop(struct pppoat_module *mod)
{
	/* TODO */
	return 0;
}

static int if_tuntap_recv(struct pppoat_module *mod, struct pppoat_packet *pkt)
{
	struct if_tuntap_ctx *ctx = mod->m_userdata;
	int                   rc;

	PPPOAT_ASSERT(if_tuntap_ctx_invariant(ctx));

	if_tun_compat_layer(ctx, pkt, false);
	rc = pppoat_io_write_sync(ctx->itc_fd, pkt->pkt_data, pkt->pkt_size);
	if (rc == 0)
		pppoat_packet_put(mod->m_pkts, pkt);

	return rc;
}

static size_t if_tun_mtu(struct pppoat_module *mod)
{
	return IF_TUN_MTU;
}

static size_t if_tap_mtu(struct pppoat_module *mod)
{
	return IF_TAP_MTU;
}

static struct pppoat_module_ops if_tun_ops = {
	.mop_init = &if_tun_init,
	.mop_fini = &if_tuntap_fini,
	.mop_run  = &if_tuntap_run,
	.mop_stop = &if_tuntap_stop,
	.mop_recv = &if_tuntap_recv,
	.mop_mtu  = &if_tun_mtu,
};

struct pppoat_module_impl pppoat_module_if_tun = {
	.mod_name  = "tun",
	.mod_descr = "TUN interface",
	.mod_type  = PPPOAT_MODULE_INTERFACE,
	.mod_ops   = &if_tun_ops,
};

static struct pppoat_module_ops if_tap_ops = {
	.mop_init = &if_tap_init,
	.mop_fini = &if_tuntap_fini,
	.mop_run  = &if_tuntap_run,
	.mop_stop = &if_tuntap_stop,
	.mop_recv = &if_tuntap_recv,
	.mop_mtu  = &if_tap_mtu,
};

struct pppoat_module_impl pppoat_module_if_tap = {
	.mod_name  = "tap",
	.mod_descr = "TAP interface",
	.mod_type  = PPPOAT_MODULE_INTERFACE,
	.mod_ops   = &if_tap_ops,
};

/* --------------------------------------------------------------------------
 *  Platform specific functions.
 * -------------------------------------------------------------------------- */
#ifdef __APPLE__

#include <net/if_utun.h>	/* UTUN_CONTROL_NAME, UTUN_OPT_IFNAME */
#include <sys/ioctl.h>		/* ioctl */
#include <sys/kern_control.h>	/* sockaddr_ctl, ctl_info */
#include <sys/socket.h>		/* socket */
#include <sys/sys_domain.h>	/* SYSPROTO_CONTROL, AF_SYS_CONTROL */

static int if_tuntap_fd_init(struct if_tuntap_ctx *ctx,
			     struct pppoat_conf   *conf,
			     enum if_tuntap_type   type)
{
	struct sockaddr_ctl addr;
	struct ctl_info     info;
	char                ifname[10] = {};
	socklen_t           ifname_len = sizeof(ifname);
	int                 fd = -1;
	int                 rc = 0;

	PPPOAT_ASSERT(type == PPPOAT_IF_TUN);

	fd = socket(PF_SYSTEM, SOCK_DGRAM, SYSPROTO_CONTROL);
	PPPOAT_ASSERT(fd >= 0); /* XXX */

	memset(&info, 0, sizeof info);
	strncpy(info.ctl_name, UTUN_CONTROL_NAME, MAX_KCTL_NAME);

	rc = ioctl(fd, CTLIOCGINFO, &info);
	PPPOAT_ASSERT(rc >= 0); /* XXX */

	addr.sc_len     = sizeof addr;
	addr.sc_family  = AF_SYSTEM;
	addr.ss_sysaddr = AF_SYS_CONTROL;
	addr.sc_id      = info.ctl_id;
	addr.sc_unit    = 0;

	rc = connect(fd, (struct sockaddr *)&addr, sizeof addr);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	rc = getsockopt(fd, SYSPROTO_CONTROL, UTUN_OPT_IFNAME,
			ifname, &ifname_len);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	(void)pppoat_io_fd_blocking_set(fd, false);

	ctx->itc_fd     = fd;
	ctx->itc_ifname = pppoat_strdup(ifname);

	return 0;
}

static void if_tuntap_fd_fini(struct if_tuntap_ctx *ctx)
{
	(void)close(ctx->itc_fd);
	pppoat_free(ctx->itc_ifname);
}

enum {
	TUN_TYPE_IP4 = 0x0800,
	TUN_TYPE_IP6 = 0x86dd,
	TUN_TYPE_IPX = 0x8137,
};

/**
 * Converts uTun format to TUN and vise versa.
 *
 * @note Conversion is done in-place. Object pkt is converted after execution.
 * @param send Defines the direction of conversion. True if uTun->TUN.
 */
static void if_tun_compat_layer(struct if_tuntap_ctx *ctx,
				struct pppoat_packet *pkt,
				bool                  send)
{
	unsigned char  *buf = pkt->pkt_data;
	unsigned short  type;
	unsigned char   pf;

	/*
	 * TUN/TAP frame format:
	 *   Flags [2 bytes]
	 *   Proto [2 bytes]
	 * See https://en.wikipedia.org/wiki/EtherType for the list of
	 * protocol types.
	 *
	 * uTun prepends protocol family to the raw packet as uint32_t.
	 * Looks like uTun supports only PF_INET and PF_INET6.
	 *
	 * So, we need to do conversion between EtherType and protocol family
	 * values. Keep the TUN/TAP flags zero.
	 */

	PPPOAT_ASSERT(pkt->pkt_size > 3);

	if (send) {
		pf = buf[3];
		switch (pf) {
		case PF_INET:
			type = TUN_TYPE_IP4;
			break;
		case PF_INET6:
			type = TUN_TYPE_IP6;
			break;
		case PF_IPX:
			type = TUN_TYPE_IPX;
			break;
		default:
			type = 0;
			pppoat_debug("tun", "Unknown PF type: %u", pf);
		}
		buf[0] = 0x00;
		buf[1] = 0x00;
		buf[2] = type >> 8;
		buf[3] = type & 0xffu;
	} else {
		type = ((unsigned short)buf[2] << 8) + buf[3];
		switch (type) {
		case TUN_TYPE_IP4:
			pf = AF_INET;
			break;
		case TUN_TYPE_IP6:
			pf = AF_INET6;
			break;
		case TUN_TYPE_IPX:
			pf = AF_IPX;
			break;
		default:
			pf = AF_UNSPEC;
			pppoat_debug("tun", "Unknown protocol type: %x", type);
		}
		buf[0] = 0x00;
		buf[1] = 0x00;
		buf[2] = 0x00;
		buf[3] = pf;
	}
}

/* -------------------------------------------------------------------------- */
#else /* __APPLE__ */

#include <sys/socket.h>		/* sockaddr required for linux/if.h */
#include <linux/if.h>		/* ifreq */
#include <linux/if_tun.h>	/* TUNSETIFF */
#include <sys/ioctl.h>		/* ioctl */
#include <fcntl.h>		/* O_RDWR */

static const char *if_tun_path = "/dev/net/tun";

static int if_tuntap_fd_init(struct if_tuntap_ctx *ctx,
			     struct pppoat_conf   *conf,
			     enum if_tuntap_type   type)
{
	struct ifreq ifr;
	int          fd;
	int          rc;

	/*
	 * We can add IFF_NO_PI to the ifr_flags. It will save 4 bytes.
	 * In this case we will have to determine protocol type
	 * in if_tun_compat_layer() for uTun frame format.
	 */

	PPPOAT_ASSERT(type == PPPOAT_IF_TUN || type == PPPOAT_IF_TAP);

	fd = open(if_tun_path, O_RDWR);
	PPPOAT_ASSERT(fd >= 0); /* XXX */

	memset(&ifr, 0, sizeof ifr);
	ifr.ifr_flags  = type == PPPOAT_IF_TUN ? IFF_TUN : IFF_TAP;
	rc = ioctl(fd, TUNSETIFF, (void *)&ifr);
	PPPOAT_ASSERT(rc >= 0); /* XXX */
	PPPOAT_ASSERT(strlen(ifr.ifr_name) > 0);

	(void)pppoat_io_fd_blocking_set(fd, false);

	ctx->itc_fd     = fd;
	ctx->itc_ifname = pppoat_strdup(ifr.ifr_name);

	return 0;
}

static void if_tuntap_fd_fini(struct if_tuntap_ctx *ctx)
{
	(void)close(ctx->itc_fd);
	pppoat_free(ctx->itc_ifname);
}

static void if_tun_compat_layer(struct if_tuntap_ctx *ctx,
				struct pppoat_packet *pkt,
				bool                  send)
{
}

#endif /* __APPLE__ */
