DEBUG_FLAGS = -g3 -Wall -Wunused -Werror
AM_CFLAGS = $(DEBUG_FLAGS) -I$(top_srcdir)/src

bin_PROGRAMS = pppoat

pppoat_SOURCES =	\
	src/conf.c	\
	src/list.c	\
	src/lock.c	\
	src/log.c	\
	src/memory.c	\
	src/pppoat.c

pppoat_SOURCES +=	\
	src/conf.h	\
	src/list.h	\
	src/lock.h	\
	src/log.h	\
	src/memory.h	\
	src/misc.h	\
	src/pppoat.h	\
	src/trace.h

pppoat_CFLAGS = $(AM_CFLAGS)
pppoat_LDFLAGS =

man_MANS = docs/pppoat.8

EXTRA_DIST = $(man_MANS) autogen.sh Doxyfile README.md
