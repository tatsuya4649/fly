#ifndef _UTIL_H
#define _UTIL_H

#include <unistd.h>
#include <string.h>

#ifndef __GNUC__
#define __attribute__((x))		/* NOTHING */
#endif

#define __unused	__attribute__((unused))

#ifdef FLY_TEST
#define __fly_static
#else
#define __fly_static			static
#endif

#define FLY_PAGESIZE			(sysconf(_SC_PAGESIZE))
#define FLY_CRLF					"\r\n"
#define FLY_CRLF_LENGTH				(strlen(FLY_CRLF))

void fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr);
#endif
