#ifndef _FLY_ERR_H
#define _FLY_ERR_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include "util.h"
#include <stdlib.h>
#include <stdio.h>
#include <stdarg.h>
#include <errno.h>
#include <string.h>
#include "alloc.h"
#include "event.h"
#include "bllist.h"

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
/* mount limit */
#define FLY_EMOUNT_LIMIT				-17
/* file limit */
#define FLY_EFILE_LIMIT					-18


#define FLY_ERR_POOL_SIZE				10

enum fly_error_level{
	/* end worker/master process */
	FLY_ERR_EMERG = 10,
	FLY_ERR_CRIT,
	/* end worker process */
	FLY_ERR_ERR,
	/* only log */
	FLY_ERR_ALERT,
	FLY_ERR_WARN,
	FLY_ERR_NOTICE,
	FLY_ERR_INFO,
	FLY_ERR_DEBUG,
};

struct fly_err{
#define FLY_ERROR_CONTENT_SIZE			200
	char				content[FLY_ERROR_CONTENT_SIZE];
	size_t				content_len;
#define FLY_ERRPTR_FOR_EMERGE_SIZE		1000
	char				*emergency_memory;

	/* -1 is invalid errno(errno is positive int) */
	int					__errno;
	struct fly_event	*event;
	fly_pool_t			*pool;

	enum fly_error_level	level;
	struct fly_bllist	blelem;

	fly_bit_t			end_process: 1;
};

typedef struct fly_err fly_err_t;
#define fly_error_level(__e)			((__e)->level)
#define fly_errlogfile_from_manager(m)	((m)->ctx->log->error->file)
#define fly_errlogfile_from_event(e)	(fly_errlogfile_manager(e)->manager)
#define fly_seterrno(err, __errno)		((err)->__errno = (__errno))
#define fly_errno(err)				((err)->__errno)
#define FLY_ERROR_LOG_LENGTH			1000
#define FLY_NULL_ERRNO					-1
#define FLY_NULL_ERRNO_DESC				"unknown error number(-1)"

struct fly_context;
void fly_errsys_init(struct fly_context  *ctx);
fly_err_t *fly_err_init(fly_pool_t *pool, int __errno, enum fly_error_level level, const char *fmt, ...);
void fly_error(struct fly_err *err, int __errno, enum fly_error_level level, const char *fmt, ...);
void fly_err_release(struct fly_err *__e);
int fly_errlog_event(fly_event_manager_t *manager, fly_err_t *err);

#define FLY_EMERGE_MEMORY_SIZE			1000
extern uint8_t fly_emerge_memory[FLY_EMERGE_MEMORY_SIZE];
/*
 * structure for printing error
 */
struct fly_errp{
	const char *content;
	int __errno;
};
typedef struct fly_errp fly_errp_t;

void fly_stdout_error(fly_errp_t *);
void fly_stderr_error(fly_errp_t *);
#define FLY_END_PROCESS(number)				exit((number))
#define FLY_ERRP_CONTENT_MAX				(100)

#define FLY_STDOUT_ERROR(fmt, ...)			do{				\
		fly_errp_t __fly_errp;								\
		fly_errc_t __errp_content[FLY_ERRP_CONTENT_MAX];	\
		snprintf(											\
			(char *) __errp_content,							\
			FLY_ERRP_CONTENT_MAX,							\
			(fmt),											\
			__VA_ARGS__										\
		);													\
		__fly_errp.content = (fly_errc_t *) __errp_content;	\
		__fly_errp.__errno = (int) errno;					\
		fly_stdout_error(&__fly_errp);						\
		FLY_END_PROCESS(1);									\
	} while(0)

#define FLY_STDERR_ERROR(fmt, ...)			do{				\
		fly_errp_t __fly_errp;								\
		const char __errp_content[FLY_ERRP_CONTENT_MAX];	\
		snprintf(											\
			(char *) __errp_content,						\
			FLY_ERRP_CONTENT_MAX,							\
			(const char *) (fmt),							\
			## __VA_ARGS__										\
		);													\
		__fly_errp.content = __errp_content;				\
		__fly_errp.__errno = (int) errno;					\
		fly_stderr_error(&__fly_errp);						\
		FLY_END_PROCESS(1);									\
	} while(0)


/* emergency error */
#define FLY_EMERGENCY_LOG_LENGTH						200
#define fly_status_str(__s)				(# __s)
enum fly_emergency_status{
	FLY_EMERGENCY_STATUS_NOMEM = 100,
	FLY_EMERGENCY_STATUS_PROCS,
	FLY_EMERGENCY_STATUS_READY,
	FLY_EMERGENCY_STATUS_ELOG,
	FLY_EMERGENCY_STATUS_NOMOUNT,
	FLY_EMERGENCY_STATUS_MODF,
};

__fly_noreturn void fly_emergency_verror(int __errno, const char *format, ...);

#define FLY_EXIT_ERROR(fmt, ...)							\
	do{														\
		int __err = errno;									\
		fly_nomem_verror(__err, (fmt), ## __VA_ARGS__);		\
	} while(0)
#define FLY_EMERGENCY_ERROR(fmt, ...)						\
	do{															\
		int __err = errno;										\
		fly_emergency_verror(__err, (fmt), ##__VA_ARGS__);	\
	} while(0)

__fly_noreturn void fly_critical_error(struct fly_err *err);
__fly_noreturn void fly_error_error(struct fly_err *err);
void fly_error_error_noexit(struct fly_err *err);
__fly_noreturn void fly_emergency_error(struct fly_err *err);
__fly_noreturn void fly_nomem_verror(__fly_unused int __errno, const char *format, ...);
void fly_alert_error(struct fly_err *err);
void fly_warn_error(struct fly_err *err);
void fly_notice_error(struct fly_err *err);
void fly_info_error(struct fly_err *err);
void fly_debug_error(struct fly_err *err);

#endif
