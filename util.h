#ifndef _UTIL_H
#define _UTIL_H

#ifndef __GNUC__
#define __attribute__((x))		/* NOTHING */
#endif

#define __unused	__attribute__((unused))

#define CRLF					"\r\n"
#define CRLF_LENGTH				(strlen(CRLF))

#endif
