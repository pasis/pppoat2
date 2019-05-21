/* pipeline.h
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

#ifndef __PPPOAT_PIPELINE_H__
#define __PPPOAT_PIPELINE_H__

#include "list.h"

#include <stdbool.h>

struct pppoat_module;
struct pppoat_packet;

struct pppoat_pipeline {
	struct pppoat_list pl_modules;
	bool               pl_is_ready;
};

int pppoat_pipeline_init(struct pppoat_pipeline *p);
void pppoat_pipeline_fini(struct pppoat_pipeline *p);

void pppoat_pipeline_add_module(struct pppoat_pipeline *p,
				struct pppoat_module   *mod);

void pppoat_pipeline_ready(struct pppoat_pipeline *p, bool ready);

int pppoat_pipeline_packet_send(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt);
int pppoat_pipeline_packet_recv(struct pppoat_pipeline *p,
				struct pppoat_module   *mod,
				struct pppoat_packet   *pkt);

#endif /* __PPPOAT_PIPELINE_H__ */
