/* module.c
 * PPP over Any Transport -- Module interface
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

#include "log.h"

#include "module.h"
#include "packet.h"
#include "pppoat.h"

/* XXX TODO check if non mandatory interface != NULL */

int pppoat_module_init(struct pppoat_module            *mod,
		       const struct pppoat_module_impl *impl,
		       struct pppoat                   *ctx)
{
	mod->m_impl = impl;
	mod->m_pkts = ctx->p_pkts;
	mod->m_invert = false;
	mod->m_userdata = NULL;

	/* XXX TODO validate impl and impl->mod_ops */

	return impl->mod_ops->mop_init(mod, ctx->p_conf);
}

void pppoat_module_fini(struct pppoat_module *mod)
{
	mod->m_impl->mod_ops->mop_fini(mod);
}

int pppoat_module_run(struct pppoat_module *mod)
{
	return mod->m_impl->mod_ops->mop_run(mod);
}

int pppoat_module_stop(struct pppoat_module *mod)
{
	return mod->m_impl->mod_ops->mop_stop(mod);
}

static void module_pkt_invert(struct pppoat_packet *pkt)
{
	if (pkt->pkt_type == PPPOAT_PACKET_SEND)
		pkt->pkt_type = PPPOAT_PACKET_RECV;
	else
		pkt->pkt_type = PPPOAT_PACKET_SEND;
}

int pppoat_module_process(struct pppoat_module  *mod,
			  struct pppoat_packet  *pkt,
			  struct pppoat_packet **next)
{
	int rc;

	/*
	 * In order to handle loopback and gateway modes, invert packets
	 * direction for respective modules.
	 */

	if (mod->m_invert && pkt != NULL)
		module_pkt_invert(pkt);

	rc = mod->m_impl->mod_ops->mop_process(mod, pkt, next);

	if (mod->m_invert && rc == 0 && *next != NULL)
		module_pkt_invert(*next);

	return rc;
}

size_t pppoat_module_mtu(struct pppoat_module *mod)
{
	return mod->m_impl->mod_ops->mop_mtu(mod);
}

enum pppoat_module_type pppoat_module_type(struct pppoat_module *mod)
{
	return mod->m_impl->mod_type;
}

const char *pppoat_module_name(struct pppoat_module *mod)
{
	return mod->m_impl->mod_name;
}

bool pppoat_module_is_blocking(struct pppoat_module *mod)
{
	return (mod->m_impl->mod_props & PPPOAT_MODULE_BLOCKING) != 0;
}
