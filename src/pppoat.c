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

#include "trace.h"

#include "conf.h"
#include "memory.h"
#include "module.h"
#include "packet.h"
#include "pipeline.h"
#include "pppoat.h"
#include "sem.h"

#include <signal.h>	/* sigaction */
#include <stdio.h>	/* fprintf */
#include <stdlib.h>	/* exit */

static struct pppoat_semaphore exit_sem;

static const pppoat_log_level_t default_log_level = PPPOAT_DEBUG;
static struct pppoat_log_driver * const default_log_drv =
						&pppoat_log_driver_stderr;

static void log_init_or_exit(struct pppoat_conf *conf)
{
	struct pppoat_log_driver *drv   = NULL;
	pppoat_log_level_t        level = 0;
	int                       rc;

	if (conf == NULL) {
		drv   = default_log_drv;
		level = default_log_level;
	}

	rc = pppoat_log_init(conf, drv, level);
	if (rc != 0) {
		/* We can't use pppoat_log(), just report to stderr. */
		fprintf(stderr, "Could not initialise %s log subsystem "
				"(rc=%d)", drv->ldrv_name, rc);
		exit(1);
	}
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

extern struct pppoat_module_impl pppoat_module_if_pppd;
extern struct pppoat_module_impl pppoat_module_tp_udp;

static void pppoat_sighandler(int signo)
{
	pppoat_debug("pppoat", "signal %d caught", signo);
	pppoat_semaphore_post(&exit_sem);
}

static struct sigaction act = {
	.sa_handler = pppoat_sighandler,
};

int main(int argc, char **argv)
{
	struct pppoat        *ctx;
	struct pppoat_module *mod;
	struct pppoat_module *mod2;
	bool                  help;
	bool                  server;
	int                   rc;

	/* First, initialise default logger to catch logging on early stages. */
	log_init_or_exit(NULL);

	pppoat_info("pppoat", "Current version is under development!");
	pppoat_info("pppoat", "You can try PPP over UDP in the following way:");
	pppoat_info("pppoat", "Make sure you have pppd (package ppp) and run the commands");
	pppoat_info("pppoat", "on the hosts you want to connect (replace port number and addresses");
	pppoat_info("pppoat", "with proper values, -s must be passed on a single host).");
	pppoat_info("pppoat", "");
	pppoat_info("pppoat", "  pppoat -s pppd.port=5000 pppd.host=192.168.1.2");
	pppoat_info("pppoat", "  pppoat pppd.port=5000 pppd.host=192.168.1.1");
	pppoat_info("pppoat", "");

	pppoat_semaphore_init(&exit_sem, 0);
	rc = sigaction(SIGTERM, &act, NULL);
	rc = rc ?: sigaction(SIGINT, &act, NULL);
	rc = rc == 0 ? 0 : P_ERR(-errno);
	PPPOAT_ASSERT(rc == 0);

	ctx = pppoat_alloc(sizeof(*ctx));
	PPPOAT_ASSERT(ctx != NULL);
	rc = pppoat_init(ctx);
	PPPOAT_ASSERT(rc == 0);

	rc = pppoat_conf_read_argv(ctx->p_conf, argc, argv);
	PPPOAT_ASSERT(rc == 0);

	pppoat_conf_find_bool(ctx->p_conf, "help", &help);
	pppoat_conf_find_bool(ctx->p_conf, "server", &server);
	if (help) {
		pppoat_conf_print_usage(argc, argv);
		goto exit;
	}
	if (server) {
		/* XXX Use default internal IPs with -s option. */
		rc = pppoat_conf_store(ctx->p_conf, "pppd.ip", "10.0.0.1:10.0.0.2");
		PPPOAT_ASSERT(rc == 0);
	}

	/* XXX Test if_pppd module. */
	mod = pppoat_alloc(sizeof *mod);
	PPPOAT_ASSERT(mod != NULL);
	mod2 = pppoat_alloc(sizeof *mod2);
	PPPOAT_ASSERT(mod2 != NULL);
	rc = pppoat_module_init(mod, &pppoat_module_if_pppd, ctx);
	PPPOAT_ASSERT(rc == 0);
	rc = pppoat_module_init(mod2, &pppoat_module_tp_udp, ctx);
	PPPOAT_ASSERT(rc == 0);
	pppoat_pipeline_add_module(ctx->p_pipeline, mod);
	pppoat_pipeline_add_module(ctx->p_pipeline, mod2);
	rc = pppoat_module_run(mod);
	PPPOAT_ASSERT(rc == 0);
	rc = pppoat_module_run(mod2);
	PPPOAT_ASSERT(rc == 0);
	pppoat_pipeline_ready(ctx->p_pipeline, true);

	pppoat_semaphore_wait(&exit_sem);

	pppoat_pipeline_ready(ctx->p_pipeline, false);
	pppoat_module_stop(mod);
	pppoat_module_stop(mod2);
	pppoat_module_fini(mod);
	pppoat_module_fini(mod2);
	pppoat_free(mod);
	pppoat_free(mod2);

exit:
	pppoat_fini(ctx);
	pppoat_free(ctx);

	pppoat_semaphore_fini(&exit_sem);

	pppoat_log_fini();

	return 0;
}
