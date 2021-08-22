#ifndef _UTIL_H
#define _UTIL_H

#include <unistd.h>
#include <string.h>

#ifndef __GNUC__
#define __attribute__((x))		/* NOTHING */
#endif

#define __unused	__attribute__((unused))

#define FLY_PAGESIZE			(sysconf(_SC_PAGESIZE))
#define CRLF					"\r\n"
#define CRLF_LENGTH				(strlen(CRLF))

void fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr);
#endif
