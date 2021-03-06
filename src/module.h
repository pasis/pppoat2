/* module.h
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

#ifndef __PPPOAT_MODULE_H__
#define __PPPOAT_MODULE_H__

#include "list.h"

struct pppoat;
struct pppoat_conf;
struct pppoat_packet;
struct pppoat_packets;
struct pppoat_pipeline;

struct pppoat_module;

#define PPPOAT_MODULE_BLOCKING 0x0001
#define PPPOAT_MODULE_STREAM   0x0002
#define PPPOAT_MODULE_PACKET   0x0004

enum pppoat_module_type {
	PPPOAT_MODULE_UNKNOWN,
	PPPOAT_MODULE_INTERFACE,
	PPPOAT_MODULE_TRANSPORT,
	PPPOAT_MODULE_PLUGIN,
};

struct pppoat_module_ops {
	int (*mop_init)(struct pppoat_module *mod, struct pppoat_conf *conf);
	void (*mop_fini)(struct pppoat_module *mod);
	int (*mop_run)(struct pppoat_module *mod);
	int (*mop_stop)(struct pppoat_module *mod);
	int (*mop_process)(struct pppoat_module  *mod,
			   struct pppoat_packet  *pkt,
			   struct pppoat_packet **next);
	size_t (*mop_mtu)(struct pppoat_module *mod);
};

struct pppoat_module_impl {
	const char               *mod_name;
	const char               *mod_descr;
	enum pppoat_module_type   mod_type;
	struct pppoat_module_ops *mod_ops;
	unsigned long             mod_props;
};

struct pppoat_module {
	const struct pppoat_module_impl *m_impl;
	struct pppoat_packets           *m_pkts;
	struct pppoat_list_link          m_link;
	uint32_t                         m_magic;
	bool                             m_invert;
	void                            *m_userdata;
};

int pppoat_module_init(struct pppoat_module            *mod,
		       const struct pppoat_module_impl *impl,
		       struct pppoat                   *ctx);
void pppoat_module_fini(struct pppoat_module *mod);

int pppoat_module_run(struct pppoat_module *mod);
int pppoat_module_stop(struct pppoat_module *mod);

int pppoat_module_process(struct pppoat_module  *mod,
			  struct pppoat_packet  *pkt,
			  struct pppoat_packet **next);

size_t pppoat_module_mtu(struct pppoat_module *mod);

enum pppoat_module_type pppoat_module_type(struct pppoat_module *mod);
const char *pppoat_module_name(struct pppoat_module *mod);
bool pppoat_module_is_blocking(struct pppoat_module *mod);

#endif /* __PPPOAT_MODULE_H__ */
