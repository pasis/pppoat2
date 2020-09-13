/* modules/tp_xmpp.c
 * PPP over Any Transport -- XMPP transport module
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

#include "config.h"
#ifdef HAVE_MODULE_XMPP

#include "trace.h"

#include "base64.h"
#include "conf.h"
#include "magic.h"
#include "memory.h"
#include "misc.h"	/* imply */
#include "module.h"
#include "packet.h"
#include "queue.h"
#include "sem.h"
#include "thread.h"

#include <stdbool.h>
#include <stdio.h>	/* snprintf */
#include <string.h>
#include <strophe.h>

/*
 * Workflow.
 *
 * Xmpp module connects to a specific jabber server with provided jid/password.
 * On connection loss or timeout, the connection handler re-connects. When
 * tp_xmpp_stop() is called, the module calls xmpp_disconnect() and finishes
 * the thread when the connection handler reports about disconnection.
 *
 * All libstrophe calls must be serialised.
 *
 * If remote_jid is not set, lock jid from the 1st stanza.
 * If remote_jid is not set and we have some kind of safe option, accept stanzas
 * only from roster.
 * If remote_jid is set, accept stanzas only from this jid.
 * If resource is set, accept stanzas only from the resource.
 * If resource is not set, lock resource from the 1st stanza.
 *
 * Congestion control.
 * Use attributes to notify how many stanzas received and handled. Stop sending
 * stanzas if there are too much pending ones.
 * Need to distinguish between congestion and connection reset from remote side.
 * Maybe we need to add signaling protocol via a specific stanza to understand
 * when to reset the congestion context.
 *
 * Ignore delayed messages.
 *
 * Send stanza only to online contact.
 */

struct tp_xmpp_ctx {
	struct pppoat_module    *txc_module;
	struct pppoat_thread     txc_thread;
	struct pppoat_queue      txc_send_q;
	struct pppoat_queue      txc_recv_q;
	struct pppoat_semaphore  txc_recv_sem;
	struct pppoat_semaphore  txc_stop_sem;
	bool                     txc_stopping;
	bool                     txc_connected;
	xmpp_ctx_t              *txc_xmpp_ctx;
	xmpp_conn_t             *txc_xmpp_conn;
	char                    *txc_jid;
	char                    *txc_passwd;
	char                    *txc_remote;
	bool                     txc_is_server;
	uint32_t                 txc_magic;
};

enum {
	TP_XMPP_MTU = 3500,
	TP_XMPP_MTU_MIN = 1500,
};

#define XMPP_LOOP_TIMEOUT 500
#define XMPP_RECONNECT_PERIOD 5000

#define XMPP_CONF_SERVER "server"
#define XMPP_CONF_JID "xmpp.jid"
#define XMPP_CONF_PASSWD "xmpp.passwd"
#define XMPP_CONF_REMOTE "xmpp.remote"

#define XMPP_NS_XEP_0091 "jabber:x:delay"
#define XMPP_NS_XEP_0203 "urn:xmpp:delay"

static const xmpp_mem_t tp_xmpp_mem;
static const xmpp_log_t tp_xmpp_log;

static void tp_xmpp_worker(struct pppoat_thread *thread);
static void tp_xmpp_conn_reconnect(struct tp_xmpp_ctx *ctx);
static void tp_xmpp_conn_handler(xmpp_conn_t * const         conn,
				 const xmpp_conn_event_t     status,
				 const int                   error,
				 xmpp_stream_error_t * const stream_error,
				 void * const                userdata);
static int tp_xmpp_message_handler(xmpp_conn_t * const   conn,
				   xmpp_stanza_t * const stanza,
				   void * const          userdata);
static int tp_xmpp_send(struct tp_xmpp_ctx *ctx, struct pppoat_packet *pkt);

static bool tp_xmpp_ctx_invariant(struct tp_xmpp_ctx *ctx)
{
	return ctx != NULL &&
	       ctx->txc_magic == PPPOAT_MODULE_TP_XMPP_MAGIC;
}

static int tp_xmpp_conf_parse(struct tp_xmpp_ctx *ctx, struct pppoat_conf *conf)
{
	int rc;

	ctx->txc_jid = NULL;
	ctx->txc_passwd = NULL;
	ctx->txc_remote = NULL;

	pppoat_conf_find_bool(conf, XMPP_CONF_SERVER, &ctx->txc_is_server);

	rc = pppoat_conf_find_string_alloc(conf, XMPP_CONF_REMOTE,
					   &ctx->txc_remote);
	/* Remote jid can be omitted on the server side. */
	if (rc == -ENOENT && ctx->txc_is_server)
		rc = 0;

	rc = rc
	  ?: pppoat_conf_find_string_alloc(conf, XMPP_CONF_JID, &ctx->txc_jid)
	  ?: pppoat_conf_find_string_alloc(conf, XMPP_CONF_PASSWD,
					   &ctx->txc_passwd);
	if (rc != 0) {
		pppoat_free(ctx->txc_jid);
		pppoat_free(ctx->txc_passwd);
		pppoat_free(ctx->txc_remote);
	}
	return rc;
}

static void tp_xmpp_conf_fini(struct tp_xmpp_ctx *ctx)
{
	pppoat_free(ctx->txc_jid);
	pppoat_free(ctx->txc_passwd);
	pppoat_free(ctx->txc_remote);
}

static int tp_xmpp_init(struct pppoat_module *mod, struct pppoat_conf *conf)
{
	struct tp_xmpp_ctx *ctx;
	int                 rc;

	ctx = pppoat_alloc(sizeof *ctx);
	if (ctx == NULL)
		return P_ERR(-ENOMEM);

	rc = tp_xmpp_conf_parse(ctx, conf);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	rc = pppoat_queue_init(&ctx->txc_send_q);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = pppoat_queue_init(&ctx->txc_recv_q);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = pppoat_thread_init(&ctx->txc_thread, &tp_xmpp_worker);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	pppoat_semaphore_init(&ctx->txc_recv_sem, 0);
	pppoat_semaphore_init(&ctx->txc_stop_sem, 0);

	xmpp_initialize();
	ctx->txc_xmpp_ctx = xmpp_ctx_new(&tp_xmpp_mem, &tp_xmpp_log);
	PPPOAT_ASSERT(ctx->txc_xmpp_ctx != NULL); /* XXX */

	ctx->txc_magic  = PPPOAT_MODULE_TP_XMPP_MAGIC;
	ctx->txc_module = mod;
	mod->m_userdata = ctx;

	return 0;
}

static void tp_xmpp_queue_flush(struct pppoat_queue *q,
				struct pppoat_module *mod)
{
	struct pppoat_packet *pkt;

	while ((pkt = pppoat_queue_dequeue(q)) != NULL)
		pppoat_packet_put(mod->m_pkts, pkt);
}

static void tp_xmpp_fini(struct pppoat_module *mod)
{
	struct tp_xmpp_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));

	xmpp_ctx_free(ctx->txc_xmpp_ctx);
	xmpp_shutdown();
	pppoat_semaphore_fini(&ctx->txc_stop_sem);
	pppoat_semaphore_fini(&ctx->txc_recv_sem);
	pppoat_thread_fini(&ctx->txc_thread);
	tp_xmpp_queue_flush(&ctx->txc_recv_q, mod);
	tp_xmpp_queue_flush(&ctx->txc_send_q, mod);
	pppoat_queue_fini(&ctx->txc_recv_q);
	pppoat_queue_fini(&ctx->txc_send_q);
	tp_xmpp_conf_fini(ctx);
	pppoat_free(ctx);
}

static void tp_xmpp_worker(struct pppoat_thread *thread)
{
	struct tp_xmpp_ctx    *ctx =
			container_of(thread, struct tp_xmpp_ctx, txc_thread);
	struct pppoat_packets *pkts;
	struct pppoat_packet  *pkt;
	int                    rc;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));

	pkts = ctx->txc_module->m_pkts;

	rc = xmpp_connect_client(ctx->txc_xmpp_conn, NULL, 0,
				 &tp_xmpp_conn_handler, ctx);
	if (rc != 0)
		tp_xmpp_conn_reconnect(ctx);

	while (true) {
		if (pppoat_semaphore_trywait(&ctx->txc_stop_sem))
			break;

		/* TODO make dynamic timeout to reduce latency on heavy load. */
		xmpp_run_once(ctx->txc_xmpp_ctx, XMPP_LOOP_TIMEOUT);
		while (ctx->txc_connected &&
		       (pkt = pppoat_queue_dequeue(&ctx->txc_send_q)) != NULL) {
			rc = tp_xmpp_send(ctx, pkt);
			PPPOAT_ASSERT(rc == 0); /* XXX */
			pppoat_packet_put(pkts, pkt);
		}
	}

	ctx->txc_stopping = true;
	if (ctx->txc_connected) {
		xmpp_disconnect(ctx->txc_xmpp_conn);
		xmpp_run(ctx->txc_xmpp_ctx);
	}
	pppoat_debug("xmpp", "Event loop finished.");
}

static int tp_xmpp_run(struct pppoat_module *mod)
{
	struct tp_xmpp_ctx *ctx = mod->m_userdata;
	int                 rc;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));

	ctx->txc_xmpp_conn = xmpp_conn_new(ctx->txc_xmpp_ctx);
	if (ctx->txc_xmpp_conn == NULL)
		return P_ERR(-ENOMEM);

	xmpp_conn_set_jid(ctx->txc_xmpp_conn, ctx->txc_jid);
	xmpp_conn_set_pass(ctx->txc_xmpp_conn, ctx->txc_passwd);
	xmpp_handler_add(ctx->txc_xmpp_conn, tp_xmpp_message_handler,
			 NULL, "message", NULL, ctx);

	ctx->txc_connected = false;
	ctx->txc_stopping = false;

	rc = pppoat_thread_start(&ctx->txc_thread);
	if (rc != 0)
		(void)xmpp_conn_release(ctx->txc_xmpp_conn);

	return rc;
}

static int tp_xmpp_stop(struct pppoat_module *mod)
{
	struct tp_xmpp_ctx *ctx = mod->m_userdata;
	int                 released;
	int                 rc;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));

	pppoat_semaphore_post(&ctx->txc_stop_sem);
	rc = pppoat_thread_join(&ctx->txc_thread);

	released = xmpp_conn_release(ctx->txc_xmpp_conn);
	PPPOAT_ASSERT(!!released);

	return rc;
}

static int tp_xmpp_pkt_get(struct pppoat_module  *mod,
			   struct pppoat_packet **pkt)
{
	struct tp_xmpp_ctx   *ctx = mod->m_userdata;

	pppoat_semaphore_wait(&ctx->txc_recv_sem);
	*pkt = pppoat_queue_dequeue(&ctx->txc_recv_q);
	if (*pkt != NULL)
		(*pkt)->pkt_type = PPPOAT_PACKET_RECV;

	return 0;
}

static int tp_xmpp_process(struct pppoat_module  *mod,
			   struct pppoat_packet  *pkt,
			   struct pppoat_packet **next)
{
	struct tp_xmpp_ctx *ctx = mod->m_userdata;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));
	PPPOAT_ASSERT(imply(pkt != NULL, pkt->pkt_type == PPPOAT_PACKET_SEND));

	if (pkt == NULL)
		return tp_xmpp_pkt_get(mod, next);

	pppoat_queue_enqueue(&ctx->txc_send_q, pkt);

	*next = NULL;
	return 0;
}

static size_t tp_xmpp_mtu(struct pppoat_module *mod)
{
	return TP_XMPP_MTU;
}

static struct pppoat_module_ops tp_xmpp_ops = {
	.mop_init    = &tp_xmpp_init,
	.mop_fini    = &tp_xmpp_fini,
	.mop_run     = &tp_xmpp_run,
	.mop_stop    = &tp_xmpp_stop,
	.mop_process = &tp_xmpp_process,
	.mop_mtu     = &tp_xmpp_mtu,
};

struct pppoat_module_impl pppoat_module_tp_xmpp = {
	.mod_name  = "xmpp",
	.mod_descr = "XMPP transport",
	.mod_type  = PPPOAT_MODULE_TRANSPORT,
	.mod_ops   = &tp_xmpp_ops,
	.mod_props = PPPOAT_MODULE_BLOCKING,
};

/* --------------------------------------------------------------------------
 *  Strophe helpers.
 * -------------------------------------------------------------------------- */

static void *tp_xmpp_mem_alloc(const size_t size, void * const userdata)
{
	(void)userdata;
	return pppoat_alloc(size);
}

static void tp_xmpp_mem_free(void *p, void * const userdata)
{
	(void)userdata;
	pppoat_free(p);
}

static void *tp_xmpp_mem_realloc(void         *p,
				 const size_t  size,
				 void * const  userdata)
{
	(void)userdata;
	return pppoat_realloc(p, size);
}

static const xmpp_mem_t tp_xmpp_mem = {
	.alloc    = &tp_xmpp_mem_alloc,
	.free     = &tp_xmpp_mem_free,
	.realloc  = &tp_xmpp_mem_realloc,
	.userdata = NULL,
};

static void tp_xmpp_log_cb(void                  *userdata,
			   const xmpp_log_level_t level,
			   const char * const     area,
			   const char * const     msg)
{
	pppoat_log_level_t l = PPPOAT_DEBUG;

	switch (level) {
	case XMPP_LEVEL_DEBUG:
		l = PPPOAT_DEBUG;
		break;
	case XMPP_LEVEL_INFO:
		l = PPPOAT_INFO;
		break;
	case XMPP_LEVEL_WARN:
		l = PPPOAT_INFO;
		break;
	case XMPP_LEVEL_ERROR:
		l = PPPOAT_ERROR;
		break;
	}
	pppoat_log(l, area, msg);
}

static const xmpp_log_t tp_xmpp_log = {
	.handler  = &tp_xmpp_log_cb,
	.userdata = NULL,
};

static const char *tp_xmpp_conn_event_to_str(xmpp_conn_event_t status)
{
	switch (status) {
	case XMPP_CONN_CONNECT:
		return "XMPP_CONN_CONNECT";
	case XMPP_CONN_DISCONNECT:
		return "XMPP_CONN_DISCONNECT";
	case XMPP_CONN_RAW_CONNECT:
		return "XMPP_CONN_RAW_CONNECT";
	case XMPP_CONN_FAIL:
		return "XMPP_CONN_FAIL";
	default:
		return "Unknown";
	}
}

static int tp_xmpp_conn_reconnect_timer_cb(xmpp_conn_t * const conn,
					   void * const        userdata)
{
	struct tp_xmpp_ctx *ctx = userdata;
	int                 rc;

	rc = xmpp_connect_client(conn, NULL, 0, &tp_xmpp_conn_handler, ctx);

	/* Keep this timed handler if (rc != 0) and retry later. */
	return rc != 0;
}

static void tp_xmpp_conn_reconnect(struct tp_xmpp_ctx *ctx)
{
	xmpp_timed_handler_delete(ctx->txc_xmpp_conn,
				  &tp_xmpp_conn_reconnect_timer_cb);
	xmpp_timed_handler_add(ctx->txc_xmpp_conn,
			       &tp_xmpp_conn_reconnect_timer_cb,
			       XMPP_RECONNECT_PERIOD, ctx);
}

static void tp_xmpp_conn_handler(xmpp_conn_t * const         conn,
				 const xmpp_conn_event_t     status,
				 const int                   error,
				 xmpp_stream_error_t * const stream_error,
				 void * const                userdata)
{
	struct tp_xmpp_ctx *ctx = userdata;
	xmpp_stanza_t      *presence;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));
	PPPOAT_ASSERT(imply(ctx->txc_stopping, status != XMPP_CONN_CONNECT));

	pppoat_debug("xmpp", "Connection handler is called for event: %s",
		     tp_xmpp_conn_event_to_str(status));

	if (status == XMPP_CONN_CONNECT)
		ctx->txc_connected = true;
	else
		ctx->txc_connected = false;

	if (ctx->txc_stopping) {
		xmpp_stop(ctx->txc_xmpp_ctx);
		return;
	}

	switch (status) {
	case XMPP_CONN_CONNECT:
		presence = xmpp_presence_new(ctx->txc_xmpp_ctx);
		PPPOAT_ASSERT(presence != NULL);
		xmpp_send(conn, presence);
		xmpp_stanza_release(presence);
		break;

	case XMPP_CONN_DISCONNECT:
		tp_xmpp_conn_reconnect(ctx);
		break;

	case XMPP_CONN_FAIL:
		pppoat_error("xmpp", "XMPP_CONN_FAIL: error=%d, stream_error=%d",
			     error, stream_error);
		break;

	default:
		PPPOAT_ASSERT(0);
	}
}

static int tp_xmpp_message_handler(xmpp_conn_t * const   conn,
				   xmpp_stanza_t * const stanza,
				   void * const          userdata)
{
	struct tp_xmpp_ctx   *ctx = userdata;
	struct pppoat_packet *pkt;
	xmpp_stanza_t        *delay;
	char                 *body;
	size_t                body_len;
	size_t                dec_len;
	int                   rc;

	PPPOAT_ASSERT(tp_xmpp_ctx_invariant(ctx));

	/* Ignore delayed messages */
	delay = xmpp_stanza_get_child_by_ns(stanza, XMPP_NS_XEP_0091);
	if (delay != NULL)
		return 1;
	delay = xmpp_stanza_get_child_by_ns(stanza, XMPP_NS_XEP_0203);
	if (delay != NULL)
		return 1;

	body = xmpp_message_get_body(stanza);
	PPPOAT_ASSERT(body != NULL); /* XXX */
	body_len = strlen(body);
	PPPOAT_ASSERT(pppoat_base64_is_valid(body, body_len)); /* XXX */

	dec_len = pppoat_base64_dec_len(body, body_len);
	pkt = pppoat_packet_get(ctx->txc_module->m_pkts,
				pppoat_max(TP_XMPP_MTU_MIN, dec_len));
	pkt->pkt_size = dec_len;
	rc = pppoat_base64_dec(body, body_len, pkt->pkt_data, dec_len);
	PPPOAT_ASSERT(rc == 0); /* XXX */

	xmpp_free(ctx->txc_xmpp_ctx, body);

	pppoat_queue_enqueue(&ctx->txc_recv_q, pkt);
	pppoat_semaphore_post(&ctx->txc_recv_sem);

	return 1;
}

enum tp_xmpp_id_t {
	XMPP_ID_UNKNOWN,
	XMPP_ID_SEND_MESSAGE,
};

enum {
	XMPP_ID_LEN_MAX = 8,
};

static char *tp_xmpp_id(struct tp_xmpp_ctx *ctx, enum tp_xmpp_id_t type)
{
	char *id;
	int   rc;

	static unsigned int counter = 0;

	/* TODO Implement this function in better way. */

	id = pppoat_alloc(XMPP_ID_LEN_MAX);
	if (id != NULL) {
		rc = snprintf(id, XMPP_ID_LEN_MAX, "id_%u", counter++);
		PPPOAT_ASSERT(rc < XMPP_ID_LEN_MAX);
	}
	return id;
}

static int tp_xmpp_send(struct tp_xmpp_ctx *ctx, struct pppoat_packet *pkt)
{
	xmpp_stanza_t *msg;
	char          *id;
	char          *base64;
	int            rc;

	id = tp_xmpp_id(ctx, XMPP_ID_SEND_MESSAGE);
	PPPOAT_ASSERT(id != NULL); /* XXX */
	msg = xmpp_message_new(ctx->txc_xmpp_ctx, "chat", ctx->txc_remote, id);
	PPPOAT_ASSERT(msg != NULL); /* XXX */
	pppoat_free(id);

	rc = pppoat_base64_enc_new(pkt->pkt_data, pkt->pkt_size, &base64);
	PPPOAT_ASSERT(rc == 0); /* XXX */
	rc = xmpp_message_set_body(msg, base64);
	PPPOAT_ASSERT(rc == XMPP_EOK); /* XXX */
	pppoat_free(base64);

	xmpp_send(ctx->txc_xmpp_conn, msg);
	xmpp_stanza_release(msg);

	return 0;
}

#endif /* HAVE_MODULE_XMPP */
