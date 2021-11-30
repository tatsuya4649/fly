#ifndef _UTIL_H
#define _UTIL_H

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>
#include <sys/types.h>
#include "../config.h"

#ifndef __GNUC__
#define __attribute__((x))		/* NOTHING */
#endif

#define __fly_unused			__attribute__((unused))
#define __fly_noreturn			__attribute__((noreturn))
#define __fly_destructor		__attribute__((destructor))
#define __fly_direct_log

#ifdef DEBUG
#define __fly_static
#else
#define __fly_static			static
#endif

#define FLY_PAGESIZE			(sysconf(_SC_PAGESIZE))
#define FLY_LF						'\n'
#define FLY_CRLF					"\r\n"
#define FLY_CRLF_LENGTH				(strlen(FLY_CRLF))

#define FLY_STRING_ARRAY(...)		(char *[]) {__VA_ARGS__}
#define fly_bit_t				unsigned int

int fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr);

#define FLY_NOT_COME_HERE		assert(0);
#define fly_unlikely(x)			(__builtin_expect(!!(x), 0))
#define fly_likely(x)			(__builtin_expect(!!(x), 1))
#define fly_unlikely_null(x)		(fly_unlikely(!(x)))
#define fly_unlikely_minus_one(x)		(fly_unlikely((x) == -1))

#define FLY_BLOCKING(res)				\
	((res) == (int) -1 && (errno == EAGAIN || errno == EWOULDBLOCK))

#define FLY_SPACE				(0x20)
#define FLY_CR					(0xD)

#ifdef FLY_GCC_COMPILER
#define fly_container_of(ptr, type , member)		({	\
		const typeof( ((type *) 0)->member ) *__p = (ptr); \
		(type *) ((char *) __p - offsetof(type, member));	\
	})
#else
#define fly_container_of(ptr, type , member)		({	\
		const typeof( ((type *) 0)->member ) *__p = (ptr); \
		(type *) ((void *) __p - offsetof(type, member));	\
	})
#endif

#ifdef WORDS_BIGENDIAN
#define FLY_BIG_ENDIAN			1
#undef FLY_LITTLE_ENDIAN
#else
#define FLY_LITTLE_ENDIAN			1
#undef FLY_BIG_ENDIAN
#endif

#define FLY_DEVNULL		("/dev/null")
#define FLY_DAEMON_STDOUT	FLY_DEVNULL
#define FLY_DAEMON_STDERR	FLY_DEVNULL
#define FLY_DAEMON_STDIN	FLY_DEVNULL

#define FLY_ROOT_DIR						"/"
#define FLY_PATH_FROM_ROOT(__p)			FLY_ROOT_DIR "/" # __p
struct fly_context;
#define FLY_DAEMON_SUCCESS					0
#define FLY_DAEMON_FORK_ERROR				-1
#define FLY_DAEMON_SETSID_ERROR				-2
#define FLY_DAEMON_CHDIR_ERROR				-3
#define FLY_DAEMON_GETRLIMIT_ERROR			-4
#define FLY_DAEMON_CLOSE_ERROR				-5
#define FLY_DAEMON_OPEN_ERROR				-6
#define FLY_DAEMON_DUP_ERROR				-7
int fly_daemon(struct fly_context *ctx);

#if defined(__GNUC__) && !defined(__clang__)
#define FLY_GCC_COMPILER		1
#else
#undef FLY_GCC_COMPILER
#endif

ssize_t fly_sendfile(int out_fd, int in_fd, off_t *offset, size_t count);

#endif
