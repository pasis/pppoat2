/* io.c
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

#include "trace.h"

#include "misc.h"

#include <errno.h>
#include <stdbool.h>
#include <sys/select.h>
#include <unistd.h>
#include <fcntl.h>

bool pppoat_io_error_is_recoverable(int error)
{
	return error == -EWOULDBLOCK ||
	       error == -EINTR       ||
	       error == -EAGAIN;
}

int pppoat_io_select(int maxfd, fd_set *rfds, fd_set *wfds)
{
	int rc;

	do {
		rc = select(maxfd + 1, rfds, wfds, NULL, NULL);
	} while (rc < 0 && errno == EINTR);
	rc = rc < 0 ? P_ERR(-errno) : 0;

	return rc;
}

int pppoat_io_select_single_read(int fd)
{
	fd_set rfds;
	int    rc;

	FD_ZERO(&rfds);
	FD_SET(fd, &rfds);
	rc = pppoat_io_select(fd, &rfds, NULL);
	PPPOAT_ASSERT(imply(rc == 0, FD_ISSET(fd, &rfds)));

	return rc;
}

int pppoat_io_select_single_write(int fd)
{
	fd_set wfds;
	int    rc;

	FD_ZERO(&wfds);
	FD_SET(fd, &wfds);
	rc = pppoat_io_select(fd, NULL, &wfds);
	PPPOAT_ASSERT(imply(rc == 0, FD_ISSET(fd, &wfds)));

	return rc;
}

int pppoat_io_write_sync(int fd, const void *buf, size_t len)
{
	ssize_t nlen = (ssize_t)len;
	ssize_t wlen;
	int     rc = 0;

	do {
		wlen = write(fd, buf, nlen);
		if (wlen < 0)
			rc = -errno;
		/* TODO Log unrecoverable I/O errors. */
		if (rc == -EINTR)
			continue;
		if (rc != 0 && pppoat_io_error_is_recoverable(rc))
			rc = pppoat_io_select_single_write(fd);
		if (wlen > 0) {
			buf   = (char *)buf + wlen;
			nlen -= wlen;
		}
	} while (rc == 0 && nlen > 0);

	return rc;
}

int pppoat_io_fd_blocking_set(int fd, bool block)
{
	int rc;

	rc = fcntl(fd, F_GETFL, NULL);
	if (rc >= 0) {
		/* rc contains flags */
		rc = block ? (rc & (~O_NONBLOCK)) : (rc | O_NONBLOCK);
		rc = fcntl(fd, F_SETFL, rc);
	}
	rc = rc == -1 ? P_ERR(-errno) : 0;

	return rc;
}

bool pppoat_io_fd_is_blocking(int fd)
{
	int  flags;
	bool blocking;

	flags = fcntl(fd, F_GETFL, NULL);
	blocking = flags >= 0 && (flags & O_NONBLOCK) == O_NONBLOCK;

	return blocking;
}
