# -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

# fly version number (used in fly/__init__.py and setup.py)
m4_define(_FLY_VERSION, 1.2.1.1)

AC_PREREQ([2.69])

AC_INIT([fly],[_FLY_VERSION])
AC_CONFIG_SRCDIR([src/master.c])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIRS([m4])
LT_PREREQ([2.4])
AC_USE_SYSTEM_EXTENSIONS

AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

build_linux=no
build_mac=no

case "$host_os" in
        linux*)
                build_linux=yes
                ;;
        darwin*)
                build_mac=yes
                ;;
	freebsd*)
		build_freebsd=yes
		;;
        *)
                AC_MSG_ERROR(OS $host_os is not supported. sorry.)
                ;;
esac

AC_MSG_NOTICE(your sysytem is $host_os)
LT_INIT() AM_INIT_AUTOMAKE([foreign]) # Checks for programs.  AC_PROG_CC([gcc]) # Checks for libraries.  # FIXME: Replace 'main' with a function in '-lbrotlidec': # FIXME: Replace 'main' with a function in '-lbrotlienc': OLD_LIBS="$LIBS"
AC_MSG_CHECKING(for brotlidec&brotlienc library works in -lbrotlidec & -lbrotlienc)
if test "$build_mac" = "yes"; then
        OLD_LDFLAGS="$LDFLAGS"
        OLD_CFLAGS="$CFLAGS"
        BROTLIDIR='/usr/local/opt/brotli'
        LDFLAGS="$LDFLAGS -L$BROTLIDIR/lib"
        CFLAGS="$CFLAGS -I$BROTLIDIR/include"
fi
AC_COMPILE_IFELSE([
	AC_LANG_PROGRAM(
		[[
#include <brotli/decode.h>
		]],
		[[
	BrotliDecoderState *state;
	char dec[10], enc[10];
	size_t available_in, available_out;
	uint8_t *next_in, *next_out;

	state = BrotliDecoderCreateInstance(0, 0, NULL);
	if (state == 0)
		return 1;

	next_in = enc;
	available_in = 10;
	next_out = dec;
	available_out = 10;
	switch(BrotliDecoderDecompressStream(
		state,
		&available_in,
		(const uint8_t **) &next_in,
		&available_out,
		&next_out,
		NULL
	)){
	case BROTLI_DECODER_RESULT_ERROR:
		return 1;
	case BROTLI_DECODER_RESULT_NEEDS_MORE_INPUT:
		return 1;
	case BROTLI_DECODER_RESULT_NEEDS_MORE_OUTPUT:
		break;
	case BROTLI_DECODER_RESULT_SUCCESS:
		break;
	}
	BrotliDecoderDestroyInstance(state);
		 ]]
)], [
		AC_COMPILE_IFELSE([
		AC_LANG_PROGRAM([[
#include <brotli/encode.h>
		 ]], [[
	BrotliEncoderOperation op;
	BrotliEncoderState *state;
	char dec[10], enc[10];
	size_t available_in, available_out;
	uint8_t *next_in, *next_out;

	state = BrotliEncoderCreateInstance(0, 0, NULL);
	if (state == 0)
		return 1;

	next_in = enc;
	available_in = 10;
	next_out = dec;
	available_out = 10;

	op = BROTLI_OPERATION_PROCESS;

	while(op != BROTLI_OPERATION_FINISH){
		op = BROTLI_OPERATION_FINISH;
		if (BrotliEncoderDecompressStream(
			state,
			&available_in,
			(const uint8_t **) &next_in,
			&available_out,
			&next_out,
			NULL
		) == BROTLI_FALSE)
			return -1;

		if (BrotliEncoderIsFinished(state) == BROTLI_TRUE)
			break;
	}
	BrotliEncoderDestroyInstance(state);
		 ]])], [
				AC_MSG_RESULT(yes)
				LIBS="-lbrotlidec -lbrotlienc $LIBS"
				AC_DEFINE([HAVE_LIBBROTLIDEC], [1], ["Define to 1 if you have the `brotlidec` library (-lbrotlidec)."])
				AC_DEFINE([HAVE_LIBBROTLIENC], [1], ["Define to 1 if you have the `brotlienc` library (-lbrotlienc)."])

				], [
			AC_MSG_RESULT(no)
			LIBS="$OLD_LIBS"
			if test "$build_mac" = "yes"; then
				LDFLAGS="$OLD_LDFLAGS"
				CFLAGS="$OLD_CFLAGS"
			fi
			])
	 ], [
	 AC_MSG_RESULT(no)
	 LIBS="$OLD_LIBS"
	if test "$build_mac" = "yes"; then
		LDFLAGS="$OLD_LDFLAGS"
		CFLAGS="$OLD_CFLAGS"
	fi
	])

# FIXME: Replace 'main' with a function in '-lcrypto':
LIBS="-lcrypto $LIBS"
AC_MSG_CHECKING(for md5 works in -lcrypto)
if test "$build_mac" = "yes"; then
	if test -d '/usr/local/opt/openssl'; then
        OPENSSLDIR='/usr/local/opt/openssl'
	elif test -d '/usr/local/opt/openssl@3'; then
        OPENSSLDIR='/usr/local/opt/openssl@3'
	elif test -d '/usr/local/opt/openssl@1.1'; then
        OPENSSLDIR='/usr/local/opt/openssl@1.1'
	else
		AC_MSG_ERROR(not found opessl directory on your system.)
	fi
        LDFLAGS="$LDFLAGS -L$OPENSSLDIR/lib"
        CFLAGS="$CFLAGS -I$OPENSSLDIR/include"
fi
AC_LINK_IFELSE([
	AC_LANG_PROGRAM(
		[[
#include <openssl/md5.h>
#include <string.h>
		 ]],[[
	MD5_CTX c;
	const char *md5_src = "Hello World!!!";
	unsigned char *__preload;
	if (MD5_Init(&c) == -1)
		return -1;

	if (MD5_Update(&c, md5_src, strlen(md5_src)) == -1)
		return -1;

	if (MD5_Final((unsigned char *) __preload, &c) == -1)
		return -1;
		]]
	)
], [
	AC_MSG_RESULT(yes)
	LIBS="-lcrypto $LIBS"
	],[
	AC_MSG_ERROR(compile error. MD5 hashing in -lcrypto can't use on your system.)
	])

# FIXME: Replace 'main' with a function in '-lpthread':
AC_CHECK_LIB([pthread], [main])
# FIXME: Replace 'main' with a function in '-lssl':
AC_CHECK_LIB([ssl], [
	SSL_CTX_new, SSL_CTX_use_certificate_file, SSL_library_init, SSL_load_error_strings, SSL_CTX_use_PrivateKey_file, SSL_CTX_check_private_key, SSL_CTX_set_options, SSL_CTX_set_alpn_select_cb
], [
	LIBS="-lssl $LIBS"
	have_ssl=yes
], [ have_ssl=no ])
if test "$have_ssl" = "no"; then
	AC_MSG_CHECKING(for SSL library works in -lssl)
	LIBS="-lssl $LIBS"
	AC_LINK_IFELSE([
		AC_LANG_PROGRAM(
			[[
			 #include <openssl/ssl.h>
			 #include <openssl/err.h>

			 int __alpn(
				SSL *ssl,
				const unsigned char **out,
				unsigned char *outlen,
				const unsigned char *in,
				unsigned int inlen, void *arg)
			{
				return 0;
			}
			]],
			[[
			 SSL_CTX *ssl_ctx;

			 SSL_library_init();
			 SSL_load_error_strings();

			 ssl_ctx = SSL_CTX_new(SSLv23_server_method());
			 SSL_CTX_use_certificate_file(ssl_ctx, "test.crt", SSL_FILETYPE_PEM);
			 SSL_CTX_use_PrivateKey_file(ssl_ctx, "test.key", SSL_FILETYPE_PEM);
			SSL_CTX_check_private_key(ssl_ctx);
			SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);
			SSL_CTX_set_alpn_select_cb(ssl_ctx, __alpn, NULL);
			 ]]
	)], [
		 AC_MSG_RESULT(yes)
		 have_ssl=yes
		 ], [
		 AC_MSG_ERROR(cmopile error. -lssl can't use on your system.)
		])
fi
if test "$have_ssl" = "yes"; then
	AC_DEFINE([HAVE_LIBSSL], [1],
		Define to 1 if you have the ssl library (-lssl).)
	LIBS="-lssl $LIBS"
else
	AC_MSG_ERROR(could not find ssl on your system)
fi


# FIXME: Replace 'main' with a function in '-lz':
#AC_CHECK_LIB([z], [inflateInit, inflateEnd, inflateInit2, deflateInit, deflateEnd, deflateInit2], [ have_z=yes ], [ have_z=no ])
LIBS="-lz $LIBS"
AC_MSG_CHECKING(for Zlib works[deflate] decoding in -lz)
if test "$build_mac" = "yes"; then
        ZLIBDIR='/usr/local/opt/zlib'
        LDFLAGS="$LDFLAGS -L$ZLIBDIR/lib"
        CFLAGS="$CFLAGS -I$ZLIBDIR/include"
fi
AC_LINK_IFELSE([
	AC_LANG_PROGRAM([[
		#include <zlib.h>
	]], [[
		 int status;
		 char enc[10];
		 char dec[10];

		 z_stream __z;
		 __z.zalloc = Z_NULL;
		 __z.zfree = Z_NULL;
		 __z.opaque = Z_NULL;
		 __z.next_in = enc;
		 __z.avail_in = 10;

		 if (inflateInit(&__z) != Z_OK)
			return 1;
		__z.avail_out = 10;
		__z.next_out = dec;

		while(status != Z_STREAM_END){
			status = inflate(&__z, Z_NO_FLUSH);
			if (status == Z_STREAM_END)
				break;
			switch(status){
			case Z_OK:
				break;
			case Z_BUF_ERROR:
				return -1;
			default:
				return -1;
			}
		}
		if (inflateEnd(&__z) != Z_OK)
			return 1;
	]])
], [AC_MSG_RESULT(yes)], [
	AC_MSG_ERROR(compile error. -lz can't use on your system.)
])
AC_MSG_CHECKING(for Zlib works[gzip] decoding in -lz)
AC_LINK_IFELSE([
	AC_LANG_PROGRAM([[
		#include <zlib.h>
	]], [[
		 int status;
		 char enc[10];
		 char dec[10];

		 z_stream __z;
		 __z.zalloc = Z_NULL;
		 __z.zfree = Z_NULL;
		 __z.opaque = Z_NULL;
		 __z.next_in = enc;
		 __z.avail_in = 10;

		 if (inflateInit2(&__z, 47) != Z_OK)
			return 1;
		__z.avail_out = 10;
		__z.next_out = dec;

		while(status != Z_STREAM_END){
			status = inflate(&__z, Z_NO_FLUSH);
			if (status == Z_STREAM_END)
				break;
			switch(status){
			case Z_OK:
				break;
			case Z_BUF_ERROR:
				return -1;
			default:
				return -1;
			}
		}
		if (inflateEnd(&__z) != Z_OK)
			return 1;
	]])
], [AC_MSG_RESULT(yes)], [
	AC_MSG_ERROR(compile error. -lz can't use on your system.)
])
AC_MSG_CHECKING(for Zlib works[deflate] encoding in -lz)
AC_LINK_IFELSE([
	AC_LANG_PROGRAM([[
		#include <zlib.h>
	]], [[
		 int status, flush;
		 char dec[10];
		 char enc[10];

		 z_stream __z;
		__z.zalloc = Z_NULL;
		__z.zfree = Z_NULL;
		__z.opaque = Z_NULL;

		if (deflateInit(&__z, Z_DEFAULT_COMPRESSION) != Z_OK)
			return 1;

		__z.next_in = dec;
		__z.avail_in = 10;
		__z.next_out = enc;
		__z.avail_out = 10;

		while(status != Z_STREAM_END){
			status = deflate(&__z, Z_FINISH);
			if (status == Z_STREAM_END)
				break;
			switch(status){
			case Z_OK:
				break;
			case Z_BUF_ERROR:
				return -1;
			default:
				return -1;
			}
		}
		if (deflateEnd(&__z) != Z_OK)
			return 1;
	]])
], [AC_MSG_RESULT(yes)], [
	AC_MSG_ERROR(compile error. -lz can't use on your system.)
])
AC_MSG_CHECKING(for Zlib works[gzip] encoding in -lz)
AC_LINK_IFELSE([
	AC_LANG_PROGRAM([[
		#include <zlib.h>
	]], [[
		 int status, flush;
		 char dec[10];
		 char enc[10];

		 z_stream __z;
		__z.zalloc = Z_NULL;
		__z.zfree = Z_NULL;
		__z.opaque = Z_NULL;

		 if (deflateInit2(&__z, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 31, 8, Z_DEFAULT_STRATEGY) != Z_OK)
			return 1;

		__z.next_in = dec;
		__z.avail_in = 10;
		__z.next_out = enc;
		__z.avail_out = 10;

		while(status != Z_STREAM_END){
			status = deflate(&__z, Z_FINISH);
			if (status == Z_STREAM_END)
				break;
			switch(status){
			case Z_OK:
				break;
			case Z_BUF_ERROR:
				return -1;
			default:
				return -1;
			}
		}
		if (deflateEnd(&__z) != Z_OK)
			return 1;
	]])
], [AC_MSG_RESULT(yes)], [
	AC_MSG_ERROR(compile error. -lz can't use on your system.)
])

# Checks for header files.
AC_CHECK_HEADERS([openssl/ssl.h openssl/md5.h zlib.h arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/ioctl.h sys/socket.h sys/time.h unistd.h setjmp.h sys/sendfile.h])

if test "$ac_cv_header_openssl_ssl_h" = "no"; then
	AC_MSG_ERROR(not found openssl/ssl.h on your system.)
fi
if test "$ac_cv_header_oepnssl_md5_h" = "no"; then
	AC_MSG_ERROR(not found openssl/md5.h on your system.)
fi
if test "$ac_cv_header_zlib" = "no"; then
	AC_MSG_ERROR(not found zlib.h on your system.)
fi

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
AC_CHECK_FUNCS([atexit dup2 gettimeofday localtime_r memset realpath setenv socket strchr strerror sigsetjmp siglongjmp strerrorname_np strerrordesc_np realpath accept4 sigqueue])

# Check for epoll or kqueue
AC_MSG_CHECKING([for epoll on your system.])
AC_CHECK_HEADER([sys/epoll.h], [
	AC_CHECK_FUNCS([epoll_create epoll_wait epoll_ctl], [
		AC_DEFINE([HAVE_EPOLL], [1], ["Define to 1 if you have the all `epoll' function."])
		AC_MSG_RESULT(yes)
		have_epoll=yes
	], [
		AC_MSG_RESULT(no)
		have_epoll=no
	])
], [
		AC_MSG_RESULT(no)
		have_epoll=no
])

if test "$have_epoll" = "no"; then
	AC_MSG_CHECKING([for kqueue on your system.])
	# check libevent
	AC_CHECK_HEADER([sys/event.h], [
		AC_CHECK_FUNCS([kqueue kevent], [
			AC_DEFINE([HAVE_KQUEUE], [1], ["Define to 1 if you have the all some `kqueue' function."])
			AC_MSG_RESULT(yes)
			have_kqueue=yes
		], [
			AC_MSG_RESULT(no)
			have_kqueue=no
		])
	], [
		AC_MSG_RESULT(no)
		have_kqueue=no
	])
fi
if test "$have_epoll" = "no" -a "$have_kqueue" = "no"; then
	AC_MSG_ERROR(epoll or kqueue can't found on your system.)
fi


# Check for signalfd
AC_CHECK_HEADER([sys/signalfd.h], [
	AC_MSG_CHECKING([ for checking if `signalfd` can be used. ])
	AC_LINK_IFELSE([
		AC_LANG_PROGRAM([
#include <sys/signalfd.h>
#include <signal.h>
		], [
			int fd;
			sigset_t fill;

			sigfillset(&fill);
			fd = signalfd(-1, &fill, SFD_NONBLOCK|SFD_CLOEXEC);
		])
	], [
		# usable signalfd
		AC_DEFINE([HAVE_SIGNALFD], [1], [Define to 1 if you have the signalfd. ])
		AC_MSG_RESULT([yes])
		have_signalfd="yes"
	], [
		AC_MSG_RESULT([no])
		# unusable signalfd
		if test have_kqueue = "no"; then
			AC_MSG_ERROR([not found signalfd or kqueue on your system.])
		fi
	])], [
		AC_MSG_RESULT([no])
		# unusable signalfd
		if test have_kqueue = "no"; then
			AC_MSG_ERROR([not found signalfd or kqueue on your system.])
		fi
	]
)

# Check for inotify
AC_CHECK_HEADER([sys/inotify.h], [
	AC_MSG_CHECKING([ for checking if `inotify` can be used. ])
	AC_LINK_IFELSE([
		AC_LANG_PROGRAM([
#include <sys/inotify.h>
#include <unistd.h>
		], [
			int fd, wd;

			fd = inotify_init();
			wd = inotify_add_watch(fd, "test_file",
				IN_ACCESS|IN_ATTRIB|IN_CLOSE_WRITE|IN_CLOSE_NOWRITE| \
					IN_CREATE|IN_DELETE|IN_DELETE_SELF|IN_MODIFY| \
					IN_MOVE_SELF|IN_MOVED_FROM|IN_MOVED_TO|IN_OPEN \
			);
			inotify_rm_watch(fd, wd);
			close(fd);
		])
	], [
		AC_DEFINE([HAVE_INOTIFY], [1], [Define to 1 if you have the inotify. ])
		AC_MSG_RESULT([yes])
		AC_CHECK_FUNCS([inotify_init1])
		have_inotify="yes"
	], [
		AC_MSG_RESULT([no])
		if test have_kqueue = "no"; then
			AC_MSG_ERROR([not found inotfy or kqueue on your system.])
		fi
	])], [
		AC_MSG_RESULT([no])
		if test have_kqueue = "no"; then
			AC_MSG_ERROR([not found inotify or kqueue on your system.])
		fi
	]
)

# check for sigsetjmp
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

AX_CHECK_COMPILE_FLAG([-std=gnu11], [CFLAGS="$CFLAGS -std=gnu11"])
AX_CHECK_COMPILE_FLAG([-W], [CFLAGS="$CFLAGS -W"])
AX_CHECK_COMPILE_FLAG([-Waddress], [CFLAGS="$CFLAGS -Waddress"])
AX_CHECK_COMPILE_FLAG([-Warray-bounds], [CFLAGS="$CFLAGS -Warray-bounds"])
AX_CHECK_COMPILE_FLAG([-Wchar-subscripts], [CFLAGS="$CFLAGS -Wchar-subscripts"])
AX_CHECK_COMPILE_FLAG([-Wenum-compare], [CFLAGS="$CFLAGS -Wenum-compare"])
AX_CHECK_COMPILE_FLAG([-Wformat], [CFLAGS="$CFLAGS -Wformat"])
AX_CHECK_COMPILE_FLAG([-Wuninitialized], [CFLAGS="$CFLAGS -Wuninitialized"])
AX_CHECK_COMPILE_FLAG([-Wmissing-braces], [CFLAGS="$CFLAGS -Wmissing-braces"])
AX_CHECK_COMPILE_FLAG([-Wnonnull], [CFLAGS="$CFLAGS -Wnonnull"])
AX_CHECK_COMPILE_FLAG([-Wparentheses], [CFLAGS="$CFLAGS -Wparentheses"])
AX_CHECK_COMPILE_FLAG([-Wimplicit], [CFLAGS="$CFLAGS -Wimplicit"])
AX_CHECK_COMPILE_FLAG([-Wsequence-point], [CFLAGS="$CFLAGS -Wsequence-point"])
AX_CHECK_COMPILE_FLAG([-Wsign-compare], [CFLAGS="$CFLAGS -Wsign-compare"])
AX_CHECK_COMPILE_FLAG([-Wstrict-overflow=1], [CFLAGS="$CFLAGS -Wstrict-overflow=1"])
AX_CHECK_COMPILE_FLAG([-Wtrigraphs], [CFLAGS="$CFLAGS -Wtrigraphs"])
AX_CHECK_COMPILE_FLAG([-Wreturn-type], [CFLAGS="$CFLAGS -Wreturn-type"])
AX_CHECK_COMPILE_FLAG([-Wunused], [CFLAGS="$CFLAGS -Wunused"])
AX_CHECK_COMPILE_FLAG([-Wswitch-default], [CFLAGS="$CFLAGS -Wswitch-default"])
AX_CHECK_COMPILE_FLAG([-Wuninitialized], [CFLAGS="$CFLAGS -Wuninitialized"])
AX_CHECK_COMPILE_FLAG([-Wmissing-field-initializers], [CFLAGS="$CFLAGS -Wmissing-field-initializers"])
AX_CHECK_COMPILE_FLAG([-Wunknown-pragmas], [CFLAGS="$CFLAGS -Wunknown-pragmas"])
AX_CHECK_COMPILE_FLAG([-Wunused-function], [CFLAGS="$CFLAGS -Wunused-function"])
AX_CHECK_COMPILE_FLAG([-Wunused-label], [CFLAGS="$CFLAGS -Wunused-label"])
AX_CHECK_COMPILE_FLAG([-Wunused-value], [CFLAGS="$CFLAGS -Wunused-value"])
AX_CHECK_COMPILE_FLAG([-Wunused-variable], [CFLAGS="$CFLAGS -Wunused-variable"])
AX_CHECK_COMPILE_FLAG([-Wunused-parameter], [CFLAGS="$CFLAGS -Wunused-parameter"])
AX_CHECK_COMPILE_FLAG([-Wvolatile-register-var], [CFLAGS="$CFLAGS -Wvolatile-register-var"])
AX_CHECK_COMPILE_FLAG([-Wcomment], [CFLAGS="$CFLAGS -Wcomment"])
AX_CHECK_COMPILE_FLAG([-Wno-deprecated], [CFLAGS="$CFLAGS -Wno-deprecated"])
AX_CHECK_COMPILE_FLAG([-Werror], [CFLAGS="$CFLAGS -Werror"])
AX_CHECK_COMPILE_FLAG([-Wno-deprecated-declarations], [CFLAGS="$CFLAGS -Wno-deprecated-declarations"])
AX_CHECK_COMPILE_FLAG([-Wcast-align], [CFLAGS="$CFLAGS -Wcast-align"])
AX_CHECK_COMPILE_FLAG([-Wtype-limits], [CFLAGS="$CFLAGS -Wtype-limits"])
AX_CHECK_COMPILE_FLAG([-Wunused-but-set-parameter], [CFLAGS="$CFLAGS -Wunused-but-set-parameter"])
AX_CHECK_COMPILE_FLAG([-Wno-error=unused-result], [CFLAGS="$CFLAGS -Wno-error=unused-result"])

AC_PREFIX_DEFAULT($PWD/fly)
AC_CONFIG_FILES([Makefile
                 src/Makefile
				 fly/__init__.py])

AC_SUBST([FLY_VERSION], [_FLY_VERSION])
AC_OUTPUT
