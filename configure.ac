AC_INIT([pppoat], [dev], [pasis.ua@gmail.com])
AM_INIT_AUTOMAKE([foreign subdir-objects])

AC_CONFIG_FILES([Makefile])

AC_PROG_CC
AM_PROG_CC_C_O

AX_PTHREAD([], [AC_MSG_ERROR([pthreads not found!])])
LIBS="$PTHREAD_LIBS $LIBS"
CFLAGS="$CFLAGS $PTHREAD_CFLAGS"

AC_OUTPUT
