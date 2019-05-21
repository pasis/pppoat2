DEBUG_FLAGS = -g3 -Wall -Wunused -Werror
AM_CFLAGS = $(DEBUG_FLAGS) -I$(top_srcdir)/src

bin_PROGRAMS = pppoat

pppoat_common_sources =	\
	src/conf.c	\
	src/conf_argv.c	\
	src/io.c	\
	src/list.c	\
	src/mutex.c	\
	src/log.c	\
	src/memory.c	\
	src/misc.c	\
	src/packet.c	\
	src/queue.c	\
	src/sem.c

pppoat_common_headers =	\
	src/conf.h	\
	src/io.h	\
	src/list.h	\
	src/mutex.h	\
	src/log.h	\
	src/magic.h	\
	src/memory.h	\
	src/misc.h	\
	src/packet.h	\
	src/queue.h	\
	src/sem.h	\
	src/trace.h

pppoat_SOURCES =		\
	$(pppoat_common_sources)\
	src/pppoat.c

pppoat_SOURCES +=		\
	$(pppoat_common_headers)\
	src/pppoat.h

pppoat_CFLAGS = $(AM_CFLAGS)
pppoat_LDFLAGS =

man_MANS = docs/pppoat.8

EXTRA_DIST = $(man_MANS) autogen.sh Doxyfile README.md

check_PROGRAMS = ut/ut
TESTS = $(check_PROGRAMS)

ut_ut_SOURCES =			\
	$(pppoat_common_sources)\
	ut/conf.c		\
	ut/list.c		\
	ut/main.c		\
	ut/packet.c		\
	ut/queue.c		\
	ut/ut.c

ut_ut_SOURCES +=		\
	$(pppoat_common_headers)\
	ut/ut.h

ut_ut_CFLAGS = $(AM_CFLAGS) -I$(top_srcdir)
ut_ut_LDFLAGS =
