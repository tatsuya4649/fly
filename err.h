#ifndef _FLY_ERR_H
#define _FLY_ERR_H

#include <stdio.h>
#include <errno.h>
#include "alloc.h"
#include "event.h"
#include "context.h"

/* errno */
#ifndef FLY_ERROR
#define FLY_ERROR(errno)		(-1*errno)
#endif
#define FLY_REQUEST_ERROR(status_code)		(-1*status_code)

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
/* not found directory */
#define FLY_ENFOUNDDIR					-15
/* buffer length error */
#define FLY_EBUFLEN						-16


/* error log event */
typedef const char fly_errc_t;
#define FLY_ERR_POOL_SIZE				10
#define FLY_ERRPTR_FOR_EMERGE_SIZE		1000

struct fly_err{
	fly_errc_t	*content;
	/* -1 is invalid errno(errno is positive int) */
	int			__errno;
	fly_event_t *event;
	fly_pool_t  *pool;

	enum {
		FLY_ERR_EMERG,
		FLY_ERR_ALERT,
		FLY_ERR_CRIT,
		FLY_ERR_ERR,
		FLY_ERR_WARN,
		FLY_ERR_NOTICE,
		FLY_ERR_INFO,
		FLY_ERR_DEBUG,
	} level;
};
typedef struct fly_err fly_err_t;
#define fly_errlogfile_from_manager(m)	((m)->ctx->log->error->file)
#define fly_errlogfile_from_event(e)	(fly_errlogfile_manager(e)->manager)
#define fly_seterrno(err, __errno)		((err)->__errno = (__errno))
#define fly_errno(err)				((err)->__errno)
#define FLY_ERROR_LOG_LENGTH			1000
#define FLY_NULL_ERRNO					-1
#define FLY_NULL_ERRNO_DESC				"unknown error number(-1)"

int fly_errsys_init(void);
fly_err_t *fly_err_init(fly_errc_t *content, int __errno, int level);
int fly_errlog_event(fly_event_manager_t *manager, fly_err_t *err);

#endif
