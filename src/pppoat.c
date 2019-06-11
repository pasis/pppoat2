/* pppoat.c
 * PPP over Any Transport -- PPPoAT context
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
#include "trace.h"

#include "conf.h"
#include "memory.h"
#include "misc.h"	/* ARRAY_SIZE, pppoat_streq */
#include "module.h"
#include "packet.h"
#include "pipeline.h"
#include "pppoat.h"
#include "sem.h"

#include <signal.h>	/* sigaction */
#include <stdio.h>	/* fprintf */
#include <stdlib.h>	/* exit */
#include <string.h>

static struct pppoat_semaphore exit_sem;

static const pppoat_log_level_t default_log_level = PPPOAT_DEBUG;
static struct pppoat_log_driver * const default_log_drv =
						&pppoat_log_driver_stderr;

/* Interface modules. */
extern struct pppoat_module_impl pppoat_module_if_pppd;
extern struct pppoat_module_impl pppoat_module_if_tun;
extern struct pppoat_module_impl pppoat_module_if_tap;
/* Transport modules. */
extern struct pppoat_module_impl pppoat_module_tp_udp;
extern struct pppoat_module_impl pppoat_module_tp_xmpp;

/* Array of all supported modules. */
struct pppoat_module_impl *pppoat_modules[] = {
	&pppoat_module_if_pppd,
	&pppoat_module_if_tun,
	&pppoat_module_if_tap,
	&pppoat_module_tp_udp,
#ifdef HAVE_MODULE_XMPP
	&pppoat_module_tp_xmpp,
#endif
};

static int log_init(struct pppoat_conf *conf)
{
	struct pppoat_log_driver *drv   = NULL;
	pppoat_log_level_t        level = 0;
	int                       rc;

	drv   = default_log_drv;
	level = default_log_level;

	/* TODO Check log driver/level in conf. */

	rc = pppoat_log_init(conf, drv, level);
	if (rc != 0) {
		/* We can't use pppoat_log(), just report to stderr. */
		fprintf(stderr, "Could not initialise %s log subsystem "
				"(rc=%d)", drv->ldrv_name, rc);
	}
	return rc;
}

int pppoat_init(struct pppoat *ctx)
{
	int rc;

	ctx->p_conf     = pppoat_alloc(sizeof *ctx->p_conf);
	ctx->p_pkts     = pppoat_alloc(sizeof *ctx->p_pkts);
	ctx->p_pipeline = pppoat_alloc(sizeof *ctx->p_pipeline);

	if (ctx->p_conf == NULL ||
	    ctx->p_pkts == NULL ||
	    ctx->p_pipeline == NULL) {
		rc = P_ERR(-ENOMEM);
		goto err_free;
	}

	rc = pppoat_conf_init(ctx->p_conf);
	if (rc != 0)
		goto err_free;
	rc = pppoat_packets_init(ctx->p_pkts);
	if (rc != 0)
		goto err_conf_fini;
	rc = pppoat_pipeline_init(ctx->p_pipeline);
	if (rc != 0)
		goto err_pkts_fini;

	return 0;

err_pkts_fini:
	pppoat_packets_fini(ctx->p_pkts);
err_conf_fini:
	pppoat_conf_fini(ctx->p_conf);
err_free:
	pppoat_free(ctx->p_pipeline);
	pppoat_free(ctx->p_pkts);
	pppoat_free(ctx->p_conf);

	PPPOAT_ASSERT(rc != 0);
	return rc;
}

void pppoat_fini(struct pppoat *ctx)
{
	pppoat_pipeline_fini(ctx->p_pipeline);
	pppoat_packets_fini(ctx->p_pkts);
	pppoat_conf_fini(ctx->p_conf);
	pppoat_free(ctx->p_pipeline);
	pppoat_free(ctx->p_pkts);
	pppoat_free(ctx->p_conf);
}

static struct pppoat_module_impl *modules_find(const char *name)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(pppoat_modules); ++i)
		if (pppoat_streq(pppoat_modules[i]->mod_name, name))
			return pppoat_modules[i];
	return NULL;
}

static void modules_print_pretty(struct pppoat_module_impl *mod)
{
	printf("%s\t- %s.\n", mod->mod_name, mod->mod_descr);
}

static void modules_print_type(enum pppoat_module_type type)
{
	size_t i;

	for (i = 0; i < ARRAY_SIZE(pppoat_modules); ++i)
		if (pppoat_modules[i]->mod_type == type ||
		    type == PPPOAT_MODULE_UNKNOWN)
			modules_print_pretty(pppoat_modules[i]);
}

static void pppoat_sighandler(int signo)
{
	pppoat_debug("pppoat", "signal %d caught", signo);
	pppoat_semaphore_post(&exit_sem);
}

static struct sigaction pppoat_sigaction = {
	.sa_handler = pppoat_sighandler,
};

static struct sigaction default_sigaction = {
	.sa_handler = SIG_DFL,
};

static void pppoat_cleanup(struct pppoat *ctx)
{
	if (ctx != NULL) {
		pppoat_fini(ctx);
		pppoat_free(ctx);
	}
	/* Restore default handlers before finalising the semaphore. */
	(void)sigaction(SIGTERM, &default_sigaction, NULL);
	(void)sigaction(SIGINT, &default_sigaction, NULL);
	pppoat_semaphore_fini(&exit_sem);
}

int main(int argc, char **argv)
{
	struct pppoat             *ctx;
	struct pppoat_module_impl *iface;
	struct pppoat_module_impl *tp;
	struct pppoat_module      *if_mod;
	struct pppoat_module      *tp_mod;
	char                      *file;
	char                      *name;
	bool                       flag;
	int                        rc;

	/* First, initialise default logger to catch logging on early stages. */
	rc = log_init(NULL);
	if (rc != 0)
		return 1;

	pppoat_info("pppoat", "Current version is under development!");
	pppoat_info("pppoat", "You can try PPP over UDP in the following way:");
	pppoat_info("pppoat", "Make sure you have pppd (package ppp) and run the commands");
	pppoat_info("pppoat", "on the hosts you want to connect (replace port number and addresses");
	pppoat_info("pppoat", "with proper values, -s must be passed on a single host).");
	pppoat_info("pppoat", "");
	pppoat_info("pppoat", "  pppoat -s udp.port=5000 udp.host=192.168.1.2");
	pppoat_info("pppoat", "  pppoat udp.port=5000 udp.host=192.168.1.1");
	pppoat_info("pppoat", "");

	pppoat_semaphore_init(&exit_sem, 0);

	rc = sigaction(SIGTERM, &pppoat_sigaction, NULL)
	  ?: sigaction(SIGINT, &pppoat_sigaction, NULL);
	rc = rc == 0 ? 0 : P_ERR(-errno);
	if (signal(SIGPIPE, SIG_IGN) == SIG_ERR)
		rc = rc ?: P_ERR(-errno);
	PPPOAT_ASSERT(rc == 0);

	ctx = pppoat_alloc(sizeof(*ctx));
	PPPOAT_ASSERT(ctx != NULL);
	rc = pppoat_init(ctx);
	PPPOAT_ASSERT(rc == 0);

	/*
	 * Read configuration from all sources.
	 */

	rc = pppoat_conf_read_argv(ctx->p_conf, argc, argv);
	PPPOAT_ASSERT(rc == 0);

	rc = pppoat_conf_find_string_alloc(ctx->p_conf, "config", &file);
	if (rc < 0 && rc != -ENOENT)
		goto exit;
	if (rc == 0) {
		rc = pppoat_conf_read_file(ctx->p_conf, file);
		pppoat_free(file);
		if (rc != 0) {
			pppoat_error("pppoat", "Couldn't read file, rc=%d", rc);
			goto exit;
		}
	}

	pppoat_conf_dump(ctx->p_conf);

	/*
	 * Re-init logging system, it may be configured via configuration.
	 */

	pppoat_log_fini();
	rc = log_init(ctx->p_conf);
	if (rc != 0) {
		pppoat_cleanup(ctx);
		return 1;
	}

	/*
	 * Print help if user asks.
	 */

	pppoat_conf_find_bool(ctx->p_conf, "help", &flag);
	if (flag) {
		pppoat_conf_print_usage(argc, argv);
		goto exit;
	}
	pppoat_conf_find_bool(ctx->p_conf, "list", &flag);
	if (flag) {
		printf("Interface modules:\n\n");
		modules_print_type(PPPOAT_MODULE_INTERFACE);
		printf("\nTransport modules:\n\n");
		modules_print_type(PPPOAT_MODULE_TRANSPORT);
		goto exit;
	}

	/*
	 * XXX Check hardcoded modules pipeline.
	 */

	pppoat_conf_find_bool(ctx->p_conf, "server", &flag);
	if (flag) {
		/* XXX Use default internal IPs with -s option. */
		rc = pppoat_conf_store(ctx->p_conf, "pppd.ip", "10.0.0.1:10.0.0.2");
		PPPOAT_ASSERT(rc == 0);
	}

	rc = pppoat_conf_find_string_alloc(ctx->p_conf, "interface", &name);
	if (rc == -ENOENT) {
		name = pppoat_strdup("pppd");
		rc = name == NULL ? P_ERR(-ENOMEM) : 0;
	}
	PPPOAT_ASSERT(rc == 0);
	iface = modules_find(name);
	PPPOAT_ASSERT(iface != NULL);
	pppoat_free(name);

	rc = pppoat_conf_find_string_alloc(ctx->p_conf, "transport", &name);
	if (rc == -ENOENT) {
		name = pppoat_strdup("udp");
		rc = name == NULL ? P_ERR(-ENOMEM) : 0;
	}
	PPPOAT_ASSERT(rc == 0);
	tp = modules_find(name);
	PPPOAT_ASSERT(tp != NULL);
	pppoat_free(name);

	PPPOAT_ASSERT(iface->mod_type == PPPOAT_MODULE_INTERFACE);
	PPPOAT_ASSERT(tp->mod_type == PPPOAT_MODULE_TRANSPORT);

	if_mod = pppoat_alloc(sizeof *if_mod);
	PPPOAT_ASSERT(if_mod != NULL);
	tp_mod = pppoat_alloc(sizeof *tp_mod);
	PPPOAT_ASSERT(tp_mod != NULL);
	rc = pppoat_module_init(if_mod, iface, ctx);
	PPPOAT_ASSERT(rc == 0);
	rc = pppoat_module_init(tp_mod, tp, ctx);
	PPPOAT_ASSERT(rc == 0);
	pppoat_pipeline_add_module(ctx->p_pipeline, if_mod);
	pppoat_pipeline_add_module(ctx->p_pipeline, tp_mod);
	rc = pppoat_module_run(if_mod);
	PPPOAT_ASSERT(rc == 0);
	rc = pppoat_module_run(tp_mod);
	PPPOAT_ASSERT(rc == 0);
	pppoat_pipeline_ready(ctx->p_pipeline, true);

	/*
	 * Wait for signal.
	 */

	pppoat_semaphore_wait(&exit_sem);

	/*
	 * Finalisation.
	 */

	pppoat_pipeline_ready(ctx->p_pipeline, false);
	pppoat_module_stop(if_mod);
	pppoat_module_stop(tp_mod);
	pppoat_module_fini(if_mod);
	pppoat_module_fini(tp_mod);
	pppoat_free(if_mod);
	pppoat_free(tp_mod);

exit:
	pppoat_cleanup(ctx);
	pppoat_log_fini();

	return 0;
}
