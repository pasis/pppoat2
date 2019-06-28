AM_CFLAGS = -I$(top_srcdir)/src

bin_PROGRAMS = pppoat

pppoat_common_sources =	\
	src/base64.c	\
	src/conf.c	\
	src/conf_argv.c	\
	src/conf_file.c	\
	src/io.c	\
	src/list.c	\
	src/log.c	\
	src/memory.c	\
	src/misc.c	\
	src/module.c	\
	src/mutex.c	\
	src/packet.c	\
	src/queue.c	\
	src/pipeline.c	\
	src/sem.c	\
	src/thread.c

pppoat_common_headers =	\
	src/base64.h	\
	src/conf.h	\
	src/io.h	\
	src/list.h	\
	src/log.h	\
	src/magic.h	\
	src/memory.h	\
	src/misc.h	\
	src/module.h	\
	src/mutex.h	\
	src/packet.h	\
	src/pipeline.h	\
	src/queue.h	\
	src/sem.h	\
	src/thread.h	\
	src/trace.h

pppoat_modules_sources =	\
	src/modules/if_pppd.c	\
	src/modules/if_tun.c	\
	src/modules/tp_udp.c	\
	src/modules/tp_xmpp.c

pppoat_SOURCES =		 \
	$(pppoat_common_sources) \
	$(pppoat_modules_sources)\
	src/pppoat.c

pppoat_SOURCES +=		\
	$(pppoat_common_headers)\
	src/pppoat.h

pppoat_CFLAGS = $(AM_CFLAGS)
pppoat_LDFLAGS =

man_MANS = docs/pppoat.8

EXTRA_DIST =		\
	$(man_MANS)	\
	autogen.sh	\
	Doxyfile	\
	README.md	\
	ut/pppoat.conf

check_PROGRAMS = ut/ut
TESTS = $(check_PROGRAMS)

ut_ut_SOURCES =			\
	$(pppoat_common_sources)\
	ut/base64.c		\
	ut/conf.c		\
	ut/list.c		\
	ut/main.c		\
	ut/packet.c		\
	ut/queue.c		\
	ut/sem.c		\
	ut/thread.c		\
	ut/trace.c		\
	ut/ut.c

ut_ut_SOURCES +=		\
	$(pppoat_common_headers)\
	ut/ut.h

ut_ut_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)
ut_ut_LDFLAGS =
