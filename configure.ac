#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

VERSION="1.1.1"
AC_INIT([fly],[VERSION])
AC_CONFIG_SRCDIR([src/master.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
LT_PREREQ([2.4])
AC_USE_SYSTEM_EXTENSIONS

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

LT_INIT()
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC([gcc])

# Checks for libraries.
# FIXME: Replace 'main' with a function in '-lbrotlidec':
AC_CHECK_LIB([brotlidec], [main])
# FIXME: Replace 'main' with a function in '-lbrotlienc':
AC_CHECK_LIB([brotlienc], [main])
# FIXME: Replace 'main' with a function in '-lcrypto':
AC_CHECK_LIB([crypto], [main], [ have_crypto=yes ], [ have_crypto=no ])
if test "$have_crypto" = "yes"; then
	AC_DEFINE([HAVE_LIBCRYPTO], [1],
		Define to 1 if you have the ssl library (-lssl).)
	LIBS="-lcrypto $LIBS"
else
	AC_MSG_ERROR(could not find crypto on your system)
fi

# FIXME: Replace 'main' with a function in '-lpthread':
AC_CHECK_LIB([pthread], [main])
# FIXME: Replace 'main' with a function in '-lssl':
AC_CHECK_LIB([ssl], [main], [ have_ssl=yes ], [ have_ssl=no ])
if test "$have_ssl" = "yes"; then
	AC_DEFINE([HAVE_LIBSSL], [1],
		Define to 1 if you have the ssl library (-lssl).)
	LIBS="-lssl $LIBS"
else
	AC_MSG_ERROR(could not find ssl on your system)
fi
# FIXME: Replace 'main' with a function in '-lz':
AC_CHECK_LIB([z], [main], [ have_z=yes ], [ have_z=no ])
if test "$have_z" = "yes"; then
	AC_DEFINE([HAVE_LIBZ], [1],
		Define to 1 if you have the ssl library (-lssl).)
	LIBS="-lz $LIBS"
else
	AC_MSG_ERROR(could not find zlib on your system)
fi

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/ioctl.h sys/socket.h sys/time.h unistd.h setjmp.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
AC_C_BIGENDIAN
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T

# Checks for library functions.
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_FUNC_MKTIME
AC_CHECK_FUNCS([atexit dup2 gettimeofday localtime_r memset realpath setenv socket strchr strerror sigsetjmp siglongjmp strerrorname_np strerrordesc_np realpath])

AC_MSG_CHECKING(for sigsetjmp)
AC_COMPILE_IFELSE(
	[AC_LANG_PROGRAM(
	 [[
#include <setjmp.h>
static sigjmp_buf senv;
	  ]],
	 [[
while(sigsetjmp(senv, 1) == 0)
	siglongjmp(senv, 1);
	  ]])],
	  [ ac_cv_func_sigsetjmp=yes ],[ ac_cv_func_sigsetjmp=no ]
)

AC_MSG_RESULT($ac_cv_func_sigsetjmp)

if test "$ac_cv_func_sigsetjmp" = "yes"; then
	AC_DEFINE(HAVE_SIGSETJMP, 1, [Define to 1 if you have the `sigsetjmp` function. ])
fi

AC_LANG_PUSH(C)
AX_CHECK_COMPILE_FLAG([-O0], [CFLAGS="$CFLAGS -O0"])
AX_CHECK_COMPILE_FLAG([-W], [CFLAGS="$CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [CFLAGS="$CFLAGS -Werror"])
AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"])
AC_LANG_POP(C)

AC_PREFIX_DEFAULT($PWD/fly)
AC_CONFIG_FILES([Makefile
                 src/Makefile
				 fly/__init__.py])

FLY_VERSION="1.1.1"
AC_SUBST([FLY_VERSION])
AC_OUTPUT
