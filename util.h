#ifndef _UTIL_H
#define _UTIL_H

#include <unistd.h>
#include <string.h>
#include <assert.h>
#include <errno.h>
#include <stddef.h>

#ifndef __GNUC__
#define __attribute__((x))		/* NOTHING */
#endif

#define __unused			__attribute__((unused))
#define __noreturn			__attribute__((noreturn))
#define __destructor		__attribute__((destructor))
#define __direct_log

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
#define fly_bit_t				int

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

#define fly_container_of(ptr, type , member)		({	\
		const typeof( ((type *) 0)->member ) *__p = (ptr); \
		(type *) ((char *) __p - offsetof(type, member));	\
	})


#endif
