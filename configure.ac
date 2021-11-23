#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ([2.69])

VERSION="1.1.2"
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
# FIXME: Replace 'main' with a function in '-lbrotlienc':
OLD_LIBS="$LIBS"
AC_MSG_CHECKING(for brotlidec&brotlienc library works in -lbrotlidec & -lbrotlienc)
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
			])
	 ], [
	 AC_MSG_RESULT(no)
	 LIBS="$OLD_LIBS"
	])

# FIXME: Replace 'main' with a function in '-lcrypto':
LIBS="-lcrypto $LIBS"
AC_MSG_CHECKING(for md5 works in -lcrypto)
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
AC_CHECK_HEADERS([openssl/ssl.h openssl/md5.h zlib.h arpa/inet.h fcntl.h inttypes.h netdb.h netinet/in.h stdint.h sys/ioctl.h sys/socket.h sys/time.h unistd.h setjmp.h])

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

FLY_VERSION="1.1.2"
AC_SUBST([FLY_VERSION])
AC_OUTPUT
