/* magic.h
 * PPP over Any Transport -- Magic constants
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

#ifndef __PPPOAT_MAGIC_H__
#define __PPPOAT_MAGIC_H__

/**
 * This file contains various constants used for sanity checks.
 *
 * An integer magic number is an uint32_t number which in HEX representation
 * consists of exactly 8 numbers and creates a human-readable word or pattern.
 * Magic number should be normalized (use capital letters for the HEX).
 */

/* packet.c::packets_cache_descr */
#define PPPOAT_PACKETS_CACHE_MAGIC 0xCAC4ED00

/* queue.c::queue_descr */
#define PPPOAT_QUEUE_MAGIC 0xC0DEC001

/* conf.c::conf_store_descr */
#define PPPOAT_CONF_STORE_MAGIC 0xD15CC0DE

/* pipeline.c::pipeline_descr */
#define PPPOAT_PIPELINE_MAGIC 0x400DF00D

/* thread.c::pppoat_thread->t_magic */
#define PPPOAT_THREAD_MAGIC 0xABBA4EAD

/* modules/pppd.c::if_pppd_ctx */
#define PPPOAT_MODULE_IF_PPPD_MAGIC 0xD00DC001

#endif /* __PPPOAT_MAGIC_H__ */
