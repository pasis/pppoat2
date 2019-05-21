/* io.h
 * PPP over Any Transport -- I/O interface
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

#ifndef __PPPOAT_IO_H__
#define __PPPOAT_IO_H__

#include <sys/select.h>	/* fd_set */
#include <stdbool.h>
#include <stddef.h>	/* size_t */

int pppoat_io_write_sync(int fd, const void *buf, size_t len);

int pppoat_io_select(int maxfd, fd_set *rfds, fd_set *wfds);
int pppoat_io_select_single_read(int fd);
int pppoat_io_select_single_write(int fd);

int pppoat_io_fd_blocking_set(int fd, bool block);
bool pppoat_io_fd_is_blocking(int fd);

#endif /* __PPPOAT_IO_H__ */
