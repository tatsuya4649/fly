#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.71])

VERSION="0.0.1"
AC_INIT([fly], [VERSION])
AC_CONFIG_SRCDIR([src/master.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
LT_PREREQ([2.4])
LT_INIT()
AM_INIT_AUTOMAKE([foreign])

# Checks for programs.
AC_PROG_CC

# Checks for libraries.
# FIXME: Replace 'main' with a function in '-lbrotlidec':
AC_CHECK_LIB([brotlidec], [main])
# FIXME: Replace 'main' with a function in '-lbrotlienc':
AC_CHECK_LIB([brotlienc], [main])
# FIXME: Replace 'main' with a function in '-lcrypto':
AC_CHECK_LIB([crypto], [main])
# FIXME: Replace 'main' with a function in '-lfly':
AC_CHECK_LIB([fly], [main])
# FIXME: Replace 'main' with a function in '-lpthread':
AC_CHECK_LIB([pthread], [main])
# FIXME: Replace 'main' with a function in '-lssl':
AC_CHECK_LIB([ssl], [main])
# FIXME: Replace 'main' with a function in '-lz':
AC_CHECK_LIB([z], [main])

# Checks for header files.
AC_CHECK_HEADERS([arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/ioctl.h sys/socket.h sys/time.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_CHECK_HEADER_STDBOOL
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
AC_CHECK_FUNCS([atexit dup2 gettimeofday localtime_r memset realpath setenv socket strchr strerror])

CFLAGS = ""
AX_CHECK_COMPILE_FLAG([-O0], [CFLAGS="$CFLAGS -O0"])
AX_CHECK_COMPILE_FLAG([-W], [CFLAGS="$CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Wall], [CFLAGS="$CFLAGS -Wall"])
AX_CHECK_COMPILE_FLAG([-Werror], [CFLAGS="$CFLAGS -Werror"])
AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"])

AC_PREFIX_DEFAULT($PWD)
AC_CONFIG_FILES([Makefile
                 src/Makefile
				 fly/__init__.py])

FLY_VERSION="0.0.1"
AC_SUBST([FLY_VERSION])
AC_OUTPUT
