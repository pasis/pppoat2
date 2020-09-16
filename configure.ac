AC_INIT([pppoat], [dev], [pasis.ua@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_FILES([Makefile])
AC_CONFIG_HEADERS([src/config.h])

AC_PROG_CC
AM_PROG_CC_C_O

AC_ARG_ENABLE([xmpp], [AS_HELP_STRING([--disable-xmpp], [disable xmpp module])])

AX_PTHREAD([], [AC_MSG_ERROR([pthreads not found!])])
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

# XMPP transport module
if test "x$enable_xmpp" != xno; then
    PKG_CHECK_MODULES([libstrophe], [libstrophe >= 0.10.0],
        [
            AC_DEFINE([HAVE_MODULE_XMPP], [1], [Build XMPP module])
            LIBS="$libstrophe_LIBS $LIBS"
            CFLAGS="$CFLAGS $libstrophe_CFLAGS"
            libstrophe_found='yes'
        ],
        [libstrophe_found='no'])
fi
if test "x$enable_xmpp" = xyes -a "x$libstrophe_found" != xyes; then
    AC_MSG_ERROR([libstrophe 0.10.0 or higher is required for xmpp module])
fi

# Debug cflags
CFLAGS="$CFLAGS -O0 -g3 -Wall -Wunused -Werror"

AC_OUTPUT
