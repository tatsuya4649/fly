#ifndef _FLY_ERR_H
#define _FLY_ERR_H

#include <errno.h>

/* errno */
#ifndef FLY_ERROR
#define FLY_ERROR(errno)		(-1*errno)
#endif

#define FLY_ERR_BUFLEN				30
/* Success */
#define FLY_SUCCESS						0
/* params error */
#define FLY_EARG						-1
/* not found error */
#define FLY_ENOTFOUND					-2
/* ca't make socket */
#define FLY_EMAKESOCK					-3
/* can't bind socket to host, ip */
#define FLY_EBINDSOCK					-4
/* can't listen socket */
#define FLY_ELISTSOCK					-5
/* conver error (text->nexwork) */
#define FLY_ECONVNET					-6
/* this version of IP is unknown */
#define FLY_EUNKNOWN_IP					-7
/* bind error list */
#define FLY_EBINDSOCK_ACCESS			-8
#define FLY_EBINDSOCK_ADDRINUSE			-9
#define FLY_EBINDSOCK_BADF				-10
#define FLY_EBINDSOCK_INVAL				-11
#define FLY_EBINDSOCK_NOTSOCK			-12
/* can't setopt */
#define FLY_ESETOPTSOCK					-13
/* incorrect host format(IPv4, IPv6) */
#define FLY_EINCADDR					-14

#endif
