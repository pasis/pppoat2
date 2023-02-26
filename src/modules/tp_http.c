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

#include "base64.h"
#include "conf.h"
#include "io.h"
#include "memory.h"
#include "misc.h"
#include "module.h"
#include "packet.h"
#include "queue.h"
#include "sem.h"
#include "thread.h"

#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <poll.h>
#include <sys/types.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

#define HTTP_CONF_PORT "http.port"
#define HTTP_CONF_REMOTE "http.remote"
#define HTTP_CONF_SERVER "server"
#define HTTP_CONF_SIDE_CHANNEL "http.side_channel"

#define HTTP_SERVER_MAX_DATA 16
#define HTTP_CLIENT_MAX_DATA 16

#define HTTP_CLOSE(fd)			\
do {					\
	if (fd >= 0) {			\
		pppoat_io_close(fd);	\
		fd = -1;		\
	}				\
} while (0)

enum {
	TP_HTTP_MTU = 1500,
	TP_HTTP_BACKLOG = 5,
	TP_HTTP_CONN_MAX = 2,
};

#define HTTP_MIN(x, y) ((x) < (y) ? (x) : (y))

struct tp_http_ctx {
	struct pppoat_module    *thc_module;
	struct pppoat_thread     thc_thread;
	struct pppoat_queue      thc_send_q;
	struct pppoat_queue      thc_recv_q;
	char                    *thc_remote_ip;
	int                      thc_sock;
	int                      thc_conn[TP_HTTP_CONN_MAX];
	int                      thc_pipe[2];
	bool                     thc_is_server;
	bool                     thc_is_side_channel;
	bool                     thc_send_ready;
	/*
	 * Side channel specific fields to split a packet into multiple
	 * HTTP messages.
	 */
	struct pppoat_packet    *thc_recv_pkt;
	unsigned                 thc_recv_size;
	unsigned                 thc_recv_offset;
	unsigned                 thc_send_offset;
};

static int tp_http_listen(struct tp_http_ctx *ctx)
{
	int fd;
	int rc;
	struct sockaddr_in sa;

	/* TODO Add IPv6 support. */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return P_ERR(-errno);

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(8080);
	rc = bind(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		rc = P_ERR(-errno);
		goto err_close;
	}

	rc = listen(fd, TP_HTTP_BACKLOG);
	if (rc) {
		rc = P_ERR(-errno);
		goto err_close;
	}

	ctx->thc_sock = fd;
	return 0;

err_close:
	close(fd);
	return rc;
}

static int tp_http_accept(struct tp_http_ctx *ctx)
{
	int rc = 0;

	ctx->thc_conn[0] = accept(ctx->thc_sock, NULL, NULL);
	rc = ctx->thc_conn[0] < 0 ? -errno: 0;
	if (rc == 0) {
		ctx->thc_conn[1] = accept(ctx->thc_sock, NULL, NULL);
		rc = ctx->thc_conn[1] < 0 ? -errno: 0;
		if (rc < 0)
			HTTP_CLOSE(ctx->thc_conn[0]);
	}
	if (rc == 0) {
		pppoat_debug("http", "Both TCP connections are established.");
	}
	return (rc == 0 || pppoat_io_error_is_recoverable(rc)) ? rc : P_ERR(rc);
}

static int tp_http_connect_single(struct tp_http_ctx *ctx)
{
	int fd;
	int rc;
	bool result;
	struct sockaddr_in sa;

	/* TODO Add IPv6 support. */
	fd = socket(AF_INET, SOCK_STREAM, 0);
	if (fd < 0)
		return P_ERR(-errno);

	memset(&sa, 0, sizeof(sa));
	sa.sin_family = AF_INET;
	sa.sin_port = htons(8080);
	result = inet_aton(ctx->thc_remote_ip, &sa.sin_addr) != 0;
	if (!result) {
		rc = P_ERR(-EINVAL);
		goto err_close;
	}
	rc = connect(fd, (struct sockaddr *)&sa, sizeof(sa));
	if (rc < 0) {
		rc = pppoat_io_error_is_recoverable(-errno) ? -errno :
							      P_ERR(-errno);
		goto err_close;
	}

	return fd;

err_close:
	close(fd);
	return rc;
}

static int tp_http_connect(struct tp_http_ctx *ctx)
{
	int rc = 0;

	rc = tp_http_connect_single(ctx);
	if (rc >= 0) {
		ctx->thc_conn[0] = rc;
		rc = tp_http_connect_single(ctx);
		ctx->thc_conn[1] = rc;
		if (rc < 0)
			HTTP_CLOSE(ctx->thc_conn[0]);
	}
	if (rc >= 0) {
		pppoat_debug("http", "Both TCP connections are established.");
		rc = 0;
	}
	return rc;
}

static bool tp_http_recv_buf_normal(struct tp_http_ctx *ctx, uint8_t *buf, ssize_t len)
{
	struct pppoat_packet *pkt;
	char                 *msg;
	char                 *body;
	char                 *field;
	size_t                body_len;
	size_t                dec_len;
	bool                  is_data_present;
	int                   rc;

	msg = pppoat_alloc(len + 1);
	PPPOAT_ASSERT(msg != NULL);
	memcpy(msg, buf, len);
	msg[len] = '\0';

	body = strstr(msg, "\r\n\r\n");
	if (body == NULL) {
		pppoat_free(msg);
		return false;
	}

	field = strstr(msg, "Content-Length:");
	is_data_present = field != NULL && (uintptr_t)field < (uintptr_t)body;
	if (is_data_present) {
		body += 4; /* Skip \r\n\r\n */
		body_len = strlen(body);
		PPPOAT_ASSERT(pppoat_base64_is_valid(body, body_len));
		pppoat_debug("RECV", "%s", body);

		dec_len = pppoat_base64_dec_len(body, body_len);
		pkt = pppoat_packet_get(ctx->thc_module->m_pkts, dec_len);
		pkt->pkt_size = dec_len;
		rc = pppoat_base64_dec(body, body_len, pkt->pkt_data, dec_len);
		PPPOAT_ASSERT(rc == 0); /* XXX */

		pppoat_queue_enqueue(&ctx->thc_recv_q, pkt);
	}

	pppoat_free(msg);
	return is_data_present;
}

static bool tp_http_recv_buf_sc(struct tp_http_ctx *ctx, uint8_t *buf, ssize_t len)
{
	struct pppoat_packet *pkt;
	char                 *msg;
	char                 *body;
	char                 *ptr;
	unsigned              size = 0;
	uint32_t              be;
	int                   rc;

	/* Side channel version of the recv_buf. */

	msg = pppoat_alloc(len + 1);
	PPPOAT_ASSERT(msg != NULL);
	memcpy(msg, buf, len);
	msg[len] = '\0';

	body = strstr(msg, "\r\n\r\n");
	if (body == NULL) {
		pppoat_free(msg);
		return false;
	}

#define HTTP_CLIENT_SIZE "GET /index.php?s="
#define HTTP_SET_COOKIE "Set-Cookie: "
#define HTTP_AUTH "Authorization: "

	pkt = ctx->thc_recv_pkt;

	ptr = msg;
	while (ptr != NULL && ptr < body) {
		if (strncmp(ptr, HTTP_CLIENT_SIZE, strlen(HTTP_CLIENT_SIZE)) == 0) {
			char *start = strstr(ptr, "?s=");
			char *end = start ? strstr(start, " ") : NULL;
			PPPOAT_ASSERT(ctx->thc_recv_pkt == NULL);
			start += 3;
			rc = pppoat_base64_dec(start, end - start, &be, sizeof(be));
			PPPOAT_ASSERT(rc == 0);
			size = ntohl(be);
			pkt = pppoat_packet_get(ctx->thc_module->m_pkts, size);
			pkt->pkt_size = size;
			size = 0;
			ctx->thc_recv_offset = 0;
		} else if (strncmp(ptr, HTTP_SET_COOKIE, strlen(HTTP_SET_COOKIE)) == 0) {
			char *start = strstr(ptr, " H=");
			char *end = strstr(ptr, "\r\n");
			if (start != NULL && end != NULL && start < end) {
				start += 3;
				rc = pppoat_base64_dec(start, strstr(start, ";") - start, &be, sizeof(be));
				PPPOAT_ASSERT(rc == 0);
				size = ntohl(be);
				pkt = pppoat_packet_get(ctx->thc_module->m_pkts, size);
				pkt->pkt_size = size;
				size = 0;
				ctx->thc_recv_offset = 0;
			}
			start = strstr(ptr, " ID=");
			if (start != NULL && end != NULL && start < end) {
				unsigned char *res;
				size_t         res_len;
				PPPOAT_ASSERT(pkt != NULL);
				start += 4;
				rc = pppoat_base64_dec_new(start, strstr(start, ";") - start, &res, &res_len);
				PPPOAT_ASSERT(rc == 0);
				size = res_len;
				memcpy(pkt->pkt_data + ctx->thc_recv_offset, res, size);
				pppoat_free(res);
			}
		} else if (strncmp(ptr, HTTP_AUTH, strlen(HTTP_AUTH)) == 0) {
			char          *start = ptr + strlen(HTTP_AUTH);
			unsigned char *res;
			size_t         res_len;
			PPPOAT_ASSERT(pkt != NULL);
			rc = pppoat_base64_dec_new(start, strstr(start, "\r\n") - start, &res, &res_len);
			PPPOAT_ASSERT(rc == 0);
			size = res_len;
			memcpy(pkt->pkt_data + ctx->thc_recv_offset, res, size);
			pppoat_free(res);
		}
		ptr = strstr(ptr, "\r\n");
		if (ptr)
			ptr += 2;
	}

	// TODO skip if pkt NULL

	ctx->thc_recv_offset += size;
	if (pkt != NULL && ctx->thc_recv_offset >= pkt->pkt_size) {
		pppoat_queue_enqueue(&ctx->thc_recv_q, pkt);
		pkt = NULL;
		ctx->thc_recv_offset = 0;
	}
	ctx->thc_recv_pkt = pkt;

	pppoat_free(msg);
	return size != 0;
}

static void tp_http_send_next_normal(struct tp_http_ctx *ctx, int fd)
{
	struct pppoat_packet *pkt;
	char                 *base64;
	char                  number[24];
	char                  buf[2048];
	int                   rc;

	pkt = pppoat_queue_dequeue(&ctx->thc_send_q);
	ctx->thc_send_ready = pkt == NULL;

	if (pkt != NULL) {
		rc = pppoat_base64_enc_new(pkt->pkt_data, pkt->pkt_size, &base64);
		PPPOAT_ASSERT(rc == 0);
		pppoat_debug("SEND", "%s", base64);

		snprintf(number, sizeof(number), "%zu", strlen(base64));

		buf[0] = '\0';
		if (ctx->thc_is_server)
			strcat(buf, "HTTP/1.1 200 OK\r\n");
		else
			strcat(buf, "POST / HTTP/1.1\r\n");
		strcat(buf, "Content-Length: ");
		strcat(buf, number);
		strcat(buf, "\r\n\r\n");
		strcat(buf, base64);
		rc = pppoat_io_write_sync(fd, buf, strlen(buf));
		PPPOAT_ASSERT(rc == 0);

		pppoat_free(base64);
		pppoat_packet_put(ctx->thc_module->m_pkts, pkt);
	}
}

static void tp_http_send_client_sc(struct tp_http_ctx *ctx, int fd)
{
	struct pppoat_packet *pkt;
	char                  buf[2048];
	char                 *b64;
	unsigned char        *ptr;
	unsigned              size;
	uint32_t              be;
	int                   rc;

	pkt = pppoat_queue_front(&ctx->thc_send_q);
	ptr = pkt->pkt_data + ctx->thc_send_offset;
	size = HTTP_MIN(HTTP_CLIENT_MAX_DATA,
			pkt->pkt_size - ctx->thc_send_offset);

	buf[0] = '\0';

	strcat(buf, "GET /index.php");
	if (ctx->thc_send_offset == 0) {
		be = htonl(pkt->pkt_size);
		rc = pppoat_base64_enc_new(&be, sizeof(be), &b64);
		PPPOAT_ASSERT(rc == 0);
		strcat(buf, "?s=");
		strcat(buf, b64);
		pppoat_free(b64);
	}
	strcat(buf, " HTTP/1.1\r\n");

	strcat(buf, "Host: ");
	strcat(buf, ctx->thc_remote_ip);
	strcat(buf, ":8080\r\n");

	strcat(buf, "User-Agent: ");
	strcat(buf, "Mozilla/5.0 (X11; Linux x86_64; rv:12.0) Gecko/20100101 "
		    "Firefox/12.0\r\n");

	strcat(buf, "Authorization: ");
	rc = pppoat_base64_enc_new(ptr, size, &b64);
	PPPOAT_ASSERT(rc == 0);
	strcat(buf, b64);
	strcat(buf, "\r\n");
	pppoat_free(b64);

	strcat(buf, "\r\n");

	rc = pppoat_io_write_sync(fd, buf, strlen(buf));
	PPPOAT_ASSERT(rc == 0);

	ctx->thc_send_offset += size;
	if (ctx->thc_send_offset >= pkt->pkt_size) {
		ctx->thc_send_offset = 0;
		pppoat_queue_pop_front(&ctx->thc_send_q);
		pppoat_packet_put(ctx->thc_module->m_pkts, pkt);
	}
}

static void tp_http_send_server_sc(struct tp_http_ctx *ctx, int fd)
{
	struct pppoat_packet *pkt;
	char                  buf[2048];
	char                 *b64;
	unsigned char        *ptr;
	unsigned              size;
	uint32_t              be;
	int                   rc;

	pkt = pppoat_queue_front(&ctx->thc_send_q);
	ptr = pkt->pkt_data + ctx->thc_send_offset;
	size = HTTP_MIN(HTTP_SERVER_MAX_DATA,
			pkt->pkt_size - ctx->thc_send_offset);

	buf[0] = '\0';

	strcat(buf, "HTTP/1.1 200 OK\r\n");
	strcat(buf, "Set-Cookie: ");
	if (ctx->thc_send_offset == 0) {
		be = htonl(pkt->pkt_size);
		rc = pppoat_base64_enc_new(&be, sizeof(be), &b64);
		PPPOAT_ASSERT(rc == 0);
		strcat(buf, "H=");
		strcat(buf, b64);
		strcat(buf, "; ");
		pppoat_free(b64);
	}
	rc = pppoat_base64_enc_new(ptr, size, &b64);
	PPPOAT_ASSERT(rc == 0);
	strcat(buf, "ID=");
	strcat(buf, b64);
	strcat(buf, "; Max-Age=3600; Version=1\r\n");
	pppoat_free(b64);

	strcat(buf, "Server: nginx/0.8.54\r\n");
	strcat(buf, "Content-Type: text/html\r\n");
	strcat(buf, "Content-Length: 107\r\n");

	strcat(buf, "\r\n");
	strcat(buf, "<html><head><title>Default page</title></head><body><center>"
		    "<h1>Server works!</h1></center></body></html>\r\n");

	rc = pppoat_io_write_sync(fd, buf, strlen(buf));
	PPPOAT_ASSERT(rc == 0);

	ctx->thc_send_offset += size;
	if (ctx->thc_send_offset >= pkt->pkt_size) {
		ctx->thc_send_offset = 0;
		pppoat_queue_pop_front(&ctx->thc_send_q);
		pppoat_packet_put(ctx->thc_module->m_pkts, pkt);
	}
}

static void tp_http_send_next_sc(struct tp_http_ctx *ctx, int fd)
{
	struct pppoat_packet *pkt;

	/* Side channel version of the send_next. */

	pkt = pppoat_queue_front(&ctx->thc_send_q);
	ctx->thc_send_ready = pkt == NULL;

	if (pkt != NULL) {
		if (ctx->thc_is_server) {
			tp_http_send_server_sc(ctx, fd);
		} else {
			tp_http_send_client_sc(ctx, fd);
		}
	}
}

static bool tp_http_recv_buf(struct tp_http_ctx *ctx, uint8_t *buf, ssize_t len)
{
	if (ctx->thc_is_side_channel) {
		return tp_http_recv_buf_sc(ctx, buf, len);
	} else {
		return tp_http_recv_buf_normal(ctx, buf, len);
	}
}

static void tp_http_send_next(struct tp_http_ctx *ctx, int fd)
{
	if (ctx->thc_is_side_channel) {
		tp_http_send_next_sc(ctx, fd);
	} else {
		tp_http_send_next_normal(ctx, fd);
	}
}

static void tp_http_send_get(struct tp_http_ctx *ctx, int fd)
{
	char buf[32];
	int rc;

	snprintf(buf, sizeof(buf), "GET / HTTP/1.1\r\n\r\n");
	rc = pppoat_io_write_sync(fd, buf, strlen(buf));
	PPPOAT_ASSERT(rc == 0);
}

static void tp_http_send_resp(struct tp_http_ctx *ctx, int fd)
{
	char buf[32];
	int rc;

	snprintf(buf, sizeof(buf), "HTTP/1.1 200 OK\r\n\r\n");
	rc = pppoat_io_write_sync(fd, buf, strlen(buf));
	PPPOAT_ASSERT(rc == 0);
}

static void tp_http_worker(struct pppoat_thread *thread)
{
	struct tp_http_ctx *ctx =
			container_of(thread, struct tp_http_ctx, thc_thread);

	struct pollfd fds[3];
	nfds_t        nfds;
	nfds_t        i;
	ssize_t       rlen;
	uint8_t       buf[2048];
	int           ready;
	bool          is_data;
	bool          running = true;

	memset(fds, 0, sizeof(fds));
	fds[0].fd = ctx->thc_conn[0];
	fds[0].events = POLLIN;
	fds[1].fd = ctx->thc_conn[1];
	fds[1].events = POLLIN;
	fds[2].fd = ctx->thc_pipe[0];
	fds[2].events = POLLIN;
	nfds = 3;

	if (!ctx->thc_is_server)
		tp_http_send_get(ctx, ctx->thc_conn[1]);

	while (running) {
		ready = poll(fds, nfds, -1);
		PPPOAT_ASSERT(ready >= 0);
		for (i = 0; i < nfds; ++i) {
			if (fds[i].revents & POLLERR)
				pppoat_debug("http", "POLLERR event.");
			if (fds[i].revents & POLLERR)
				pppoat_debug("http", "POLLHUP event.");

			if (fds[i].revents & POLLIN &&
			    fds[i].fd == ctx->thc_pipe[0]) {
				/* This is the condition to stop the module. */
				running = false;
				break;
			}

			if (fds[i].revents & POLLIN) {
				rlen = read(fds[i].fd, buf, sizeof(buf));
				PPPOAT_ASSERT(rlen >= 0);
				if (rlen == 0)
					continue;
				is_data = tp_http_recv_buf(ctx, buf, rlen);
				if (!is_data) {
					tp_http_send_next(ctx, fds[i].fd);
				} else {
					if (ctx->thc_is_server)
						tp_http_send_resp(ctx, fds[i].fd);
					else
						tp_http_send_get(ctx, fds[i].fd);
				}
			}
		}
	}
}

static void tp_http_close_sockets(struct tp_http_ctx *ctx)
{
	HTTP_CLOSE(ctx->thc_pipe[0]);
	HTTP_CLOSE(ctx->thc_pipe[1]);
	HTTP_CLOSE(ctx->thc_conn[0]);
	HTTP_CLOSE(ctx->thc_conn[1]);
	HTTP_CLOSE(ctx->thc_sock);
}

static int tp_http_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct tp_http_ctx *ctx;
	int                 i;
	int                 rc;

	ctx = pppoat_alloc(sizeof(*ctx));
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	memset(ctx, 0, sizeof(*ctx));
	ctx->thc_module = mod;
	ctx->thc_sock = -1;
	ctx->thc_pipe[0] = -1;
	ctx->thc_pipe[1] = -1;
	for (i = 0; i < TP_HTTP_CONN_MAX; ++i)
		ctx->thc_conn[i] = -1;

	pppoat_conf_find_bool(conf, HTTP_CONF_SERVER, &ctx->thc_is_server);
	pppoat_conf_find_bool(conf, HTTP_CONF_SIDE_CHANNEL,
			      &ctx->thc_is_side_channel);

	rc = pppoat_conf_find_string_alloc(conf, HTTP_CONF_REMOTE,
					   &ctx->thc_remote_ip);
	if (rc == -ENOENT && !ctx->thc_is_server)
		pppoat_debug("http", "'" HTTP_CONF_REMOTE "' is mandatory.");
	PPPOAT_ASSERT(ctx->thc_is_server || rc == 0); /* XXX */

	rc = pppoat_queue_init(&ctx->thc_send_q);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = pppoat_queue_init(&ctx->thc_recv_q);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = pppoat_thread_init(&ctx->thc_thread, &tp_http_worker);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	ctx->thc_send_ready = !ctx->thc_is_server;

	mod->m_userdata = ctx;

	return 0;
}

static void tp_http_fini(struct pppoat_module *mod)
{
	struct tp_http_ctx   *ctx = mod->m_userdata;
	struct pppoat_packet *pkt;

	while ((pkt = pppoat_queue_dequeue(&ctx->thc_recv_q)))
		pppoat_packet_put(mod->m_pkts, pkt);
	while ((pkt = pppoat_queue_dequeue(&ctx->thc_send_q)))
		pppoat_packet_put(mod->m_pkts, pkt);
	if (ctx->thc_recv_pkt)
		pppoat_packet_put(mod->m_pkts, ctx->thc_recv_pkt);

	pppoat_thread_fini(&ctx->thc_thread);
	/* TODO Flush queues. */
	pppoat_queue_fini(&ctx->thc_recv_q);
	pppoat_queue_fini(&ctx->thc_send_q);
	pppoat_free(ctx->thc_remote_ip);

	pppoat_free(ctx);
}

static int tp_http_run(struct pppoat_module *mod)
{
	struct tp_http_ctx *ctx = mod->m_userdata;
	int                 rc;

	rc = pipe(ctx->thc_pipe);
	if (rc < 0)
		return P_ERR(-errno);

	if (ctx->thc_is_server) {
		rc = tp_http_listen(ctx)
		  ?: tp_http_accept(ctx);
	} else {
		rc = tp_http_connect(ctx);
	}
	if (rc == 0)
		rc = pppoat_thread_start(&ctx->thc_thread);
	if (rc != 0)
		tp_http_close_sockets(ctx);

	return rc;
}

static int tp_http_stop(struct pppoat_module *mod)
{
	struct tp_http_ctx *ctx = mod->m_userdata;
	int                 rc;

	pppoat_debug("http", "Stopping http module.");

	rc = write(ctx->thc_pipe[1], "x", 1);
	PPPOAT_ASSERT(rc == 1);
	rc = pppoat_thread_join(&ctx->thc_thread);

	tp_http_close_sockets(ctx);

	return rc;
}

static int tp_http_pkt_recv(struct tp_http_ctx    *ctx,
			    struct pppoat_packet **pkt)
{
	*pkt = pppoat_queue_dequeue(&ctx->thc_recv_q);
	if (*pkt != NULL)
		(*pkt)->pkt_type = PPPOAT_PACKET_RECV;
	return 0;
}

static int tp_http_pkt_send(struct tp_http_ctx *ctx, struct pppoat_packet *pkt)
{
	pppoat_queue_enqueue(&ctx->thc_send_q, pkt);

	if (ctx->thc_send_ready)
		tp_http_send_next(ctx, ctx->thc_is_server ? ctx->thc_conn[1] : ctx->thc_conn[0]);

	return 0;
}

static int tp_http_process(struct pppoat_module  *mod,
			   struct pppoat_packet  *pkt,
			   struct pppoat_packet **next)
{
	struct tp_http_ctx *ctx = mod->m_userdata;
	int                 rc;

	PPPOAT_ASSERT(imply(pkt != NULL, pkt->pkt_type == PPPOAT_PACKET_SEND));

	if (pkt == NULL) {
		return tp_http_pkt_recv(ctx, next);
	}

	rc = tp_http_pkt_send(ctx, pkt);

	*next = NULL;
	return rc;
}

static size_t tp_http_mtu(struct pppoat_module *mod)
{
	return TP_HTTP_MTU;
}

static struct pppoat_module_ops tp_http_ops = {
	.mop_init    = &tp_http_init,
	.mop_fini    = &tp_http_fini,
	.mop_run     = &tp_http_run,
	.mop_stop    = &tp_http_stop,
	.mop_process = &tp_http_process,
	.mop_mtu     = &tp_http_mtu,
};

struct pppoat_module_impl pppoat_module_tp_http = {
	.mod_name  = "http",
	.mod_descr = "HTTP transport",
	.mod_type  = PPPOAT_MODULE_TRANSPORT,
	.mod_ops   = &tp_http_ops,
	.mod_props = PPPOAT_MODULE_BLOCKING,
};
