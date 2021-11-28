#ifndef _EVENT_H
#define _EVENT_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <stdio.h>
#include <stdbool.h>
#include "../config.h"

#ifdef HAVE_EPOLL
/* for linux */
#include <sys/epoll.h>
#elif defined HAVE_KQUEUE
/* for macos */
#include <sys/event.h>
/* other system */
#else
#error not found C header for "event"on your system.
#endif

#include <sys/time.h>
#include "alloc.h"
#include "util.h"
#include  "rbtree.h"
#include "queue.h"

extern fly_pool_t *fly_event_pool;
#define FLY_EVENT_POOL_SIZE			100

#ifdef HAVE_EPOLL
#define FLY_READ		EPOLLIN
#define FLY_WRITE		EPOLLOUT
#elif defined HAVE_KQUEUE
#define FLY_READ		(1<<0)
#define FLY_WRITE		(1<<1)	
//#define FLY_READ		EVFILT_READ
//#define FLY_WRITE		EVFILT_WRITE
//#define FLY_KQ_SIGNAL	(1<<3)
#define FLY_KQ_INOTIFY	(1<<4)
#endif

#define FLY_EVLIST_ELES			1000

typedef struct fly_context fly_context_t;
struct fly_event_manager{
	fly_pool_t					*pool;
	fly_context_t				*ctx;
	int							efd;
#ifdef HAVE_EPOLL
	struct epoll_event			*evlist;
#elif defined HAVE_KQUEUE
	struct kevent				*evlist;
#endif
	int							maxevents;

	struct fly_rb_tree			*rbtree;
	struct fly_queue			monitorable;
	struct fly_queue			unmonitorable;

	/* if event occurred error termination the process, called */
	void (*jbend_handle)(fly_context_t *ctx);
};
typedef struct fly_event_manager fly_event_manager_t;

typedef struct timeval fly_time_t;
struct __fly_event_for_rbtree{
	fly_time_t				*abs_timeout;
	struct fly_event		*ptr;
};
#define FLY_EVENT_FOR_RBTREE_INIT(__e)		\
	do{	\
		(__e)->rbtree_elem.abs_timeout = &(__e)->abs_timeout;		\
		(__e)->rbtree_elem.ptr = (__e);		\
	} while(0)

#ifdef HAVE_EPOLL
	typedef  epoll_data_t	fly_event_data_t;
#elif defined HAVE_KQUEUE
	typedef  void			*fly_event_data_t;
#else
#error not defined HAVE_EPOLL AND HAVE_KQUEUE
#endif

struct fly_err;
struct fly_event{
	fly_event_manager_t				*manager;
	int								fd;
#ifdef DEBUG
	int 						post_fd;
#endif
#ifdef HAVE_KQUEUE
	int								id;
#endif
	int								read_or_write;
#ifdef HAVE_KQUEUE
	/* for saving previous read_or_write */
	int								post_row;
#define FLY_NO_POST_ROW				999
#endif
	int 							available_row;
	int 							eflag;

	fly_time_t						timeout;
	fly_time_t 						spawn_time;
	fly_time_t 						start;
	fly_time_t 						abs_timeout;
	int								tflag;

	int								flag;

	struct __fly_event_for_rbtree	rbtree_elem;
	/* for manager events list */
	struct fly_queue				qelem;
	/* for manager unmonitorable list */
	struct fly_queue				uqelem;

	struct fly_rb_node				*rbnode;

	int								(*handler)(struct fly_event *);
	int								(*end_handler)(struct fly_event *);
	int								(*expired_handler)(struct fly_event *);
	char							*handler_name;

	void							*event_data;
	void							*end_event_data;
	void							*expired_event_data;
	void 							*event_fase;
	void 							*event_state;
	uint8_t							*emerge_ptr;

	/*
	 * if event handler fail, this function is called.
	 * this function must close fd(event file).
	 */
	int								(*fail_close)(struct fly_event *e, int fd);

	struct fly_bllist				errors;
	size_t							err_count;

#ifdef HAVE_KQUEUE
	sigset_t						sigset;
#endif

	/* event bit fields */
	fly_bit_t						file_type: 4;
	fly_bit_t 						expired: 1;
	fly_bit_t 						available: 1;
	fly_bit_t						yetadd: 1;
	fly_bit_t						if_fail_term: 1;
};

typedef int (fly_event_handler_t)(struct fly_event *);
#ifdef DEBUG
__fly_unused static struct fly_event *fly_event_debug(struct fly_queue*__q)
{
	return (struct fly_event *) fly_queue_data(__q, struct fly_event, qelem);
}
#endif

#define fly_context_from_event(__e)					\
				((__e)->manager->ctx)
#define FLY_POOL_MANAGER_FROM_EVENT(__e)			\
			((__e)->manager->ctx->pool_manager)
#define FLY_EVENT_HANDLER(e, __handler)	\
	do{									\
		(e)->handler = (__handler);		\
		(e)->handler_name = #__handler;	\
	} while(0)

#define FLY_EVENT_EXPIRED_END_HANDLER(e, __handler, data)		\
		do{			\
			FLY_EVENT_END_HANDLER(e, __handler, data);		\
			FLY_EVENT_EXPIRED_HANDLER(e, __handler, data);	\
		} while(0)

#define FLY_EVENT_END_HANDLER(e, __handler, __data)	\
	do{									\
		(e)->end_handler = (__handler);		\
		(e)->end_event_data = (void *) (__data);	\
	} while(0)
#define FLY_EVENT_EXPIRED_HANDLER(e, __handler, __data)	\
	do{									\
		(e)->expired_handler = (__handler);		\
		(e)->expired_event_data = (void *) (__data);	\
	} while(0)

typedef struct fly_event fly_event_t;
#define fly_time(tptr)		gettimeofday((struct timeval *) tptr, NULL)

#define fly_time_null(t)									\
	do {														\
		(t).tv_sec = -1;									\
		(t).tv_usec = -1;									\
	} while(0)
#define fly_time_zero(t)									\
	do {														\
		(t).tv_sec = 0;									\
		(t).tv_usec = 0;									\
	} while(0)
#define FLY_TIME_NULL			{ .tv_sec=-1, .tv_usec=-1 }
#define is_fly_time_null(tptr)	\
	((tptr)->tv_sec == -1 && (tptr)->tv_usec == -1 )

#define fly_equal_time(t1, t2)								\
	do{														\
		(t1).tv_sec = (t2).tv_sec;							\
		(t1).tv_usec = (t2).tv_usec;						\
	} while(0)
#define fly_diff_sec(new, old)	((new).tv_sec-(old).tv_sec)
#define fly_diff_usec(new, old)	((float) ((new).tv_usec-(old).tv_usec)/(1000.0*1000.0))

/* eflag(epoll flag) shown detail in epoll API */
/* tflag(time flag) */
#define FLY_INFINITY	1<<0
#define FLY_TIMEOUT		1<<1
#define FLY_WAITFIRST	1<<2
#define FLY_INHERIT		1<<3
#define fly_not_infinity(eptr)		(!((eptr)->tflag & FLY_INFINITY))
/* flag(generary flag) */
#define FLY_PERSISTENT	1<<0
#define FLY_NODELETE	1<<1
#define FLY_MODIFY		1<<2
#define FLY_TIMER_NOW	1<<3
#define FLY_CLOSE_EV	1<<4
#define fly_nodelete(e)						\
	(										\
		((e)->flag & FLY_PERSISTENT)	||	\
		((e)->flag & FLY_NODELETE)		||	\
		((e)->flag & FLY_MODIFY)			\
	)
int fly_event_inherit_register(fly_event_t *e);
/* monitored event file type(4bit field) */
#define __FLY_REGULAR		0x00
#define __FLY_DIRECTORY		0x01
#define __FLY_SOCKET		0x02
#define __FLY_FIFO			0x03
#define __FLY_DEVICE		0x04
#define __FLY_SLINK			0x05
#define __FLY_EPOLL			0x06
#define __FLY_SIGNAL		0x07
#define __FLY_INOTIFY		0x08

#define	fly_event_is_file_type(e, type)	((e)->file_type == __FLY_ ## type)
#define fly_event_file_type(e, type)			((e)->file_type = __FLY_ ## type)

#define fly_event_is_regular(e)		fly_event_is_file_type((e), REGULAR)
#define fly_event_is_dir(e)			fly_event_is_file_type((e), DIRECTORY)
#define fly_event_is_socket(e)		fly_event_is_file_type((e), SOCKET)
#define fly_event_is_fifo(e)		fly_event_is_file_type((e), FIFO)
#define fly_event_is_device(e)		fly_event_is_file_type((e), DEVICE)
#define fly_event_is_symlink(e)		fly_event_is_file_type((e), SLINK)
#define fly_event_is_epoll(e)		fly_event_is_file_type((e), EPOLL)
#define fly_event_is_signal(e)		fly_event_is_file_type((e), SIGNAL)
#define fly_event_is_inotify(e)		fly_event_is_file_type((e), INOTIFY)

#define fly_event_regular(e)		fly_event_file_type((e), REGULAR)
#define fly_event_dir(e)			fly_event_file_type((e), DIRECTORY)
#define fly_event_socket(e)			fly_event_file_type((e), SOCKET)
#define fly_event_fifo(e)			fly_event_file_type((e), FIFO)
#define fly_event_device(e)			fly_event_file_type((e), DEVICE)
#define fly_event_symlink(e)		fly_event_file_type((e), SLINK)
#define fly_event_epoll(e)			fly_event_file_type((e), EPOLL)
#define fly_event_signal(e)			fly_event_file_type((e), SIGNAL)
#define fly_event_inotify(e)		fly_event_file_type((e), INOTIFY)

#define fly_event_monitorable(e)	\
	(!fly_event_is_regular((e)) && !fly_event_is_dir((e)))
#define fly_event_unmonitorable(e)	(!(fly_event_monitorable((e))))

#ifdef HAVE_EPOLL
#define FLY_EVENT_CTL_ADD			EPOLL_CTL_ADD
#define FLY_EVENT_CTL_MOD			EPOLL_CTL_MOD
#elif defined HAVE_KQUEUE
#define FLY_EVENT_CTL_ADD			0
#define FLY_EVENT_CTL_MOD			1
#endif

#define fly_event_op(__e)			\
	((__e)->flag&FLY_MODIFY) ? FLY_EVENT_CTL_MOD: FLY_EVENT_CTL_ADD
#define fly_event_already_added(__e)	\
	(!(__e)->yetadd)

#define FLY_EVENT_HANDLE_FAILURE	(-1)
#define FLY_EVENT_HANDLE_FAILURE_LOG_MAXLEN		100

/* manager setting */
fly_event_manager_t *fly_event_manager_init(fly_context_t *ctx);
int fly_event_manager_release(fly_event_manager_t *manager);
#define FLY_EVENT_HANDLER_INVALID_MANAGER			-2
#define FLY_EVENT_HANDLER_EPOLL_ERROR				-3
#define FLY_EVENT_HANDLER_EXPIRED_EVENT_ERROR		-4
int fly_event_handler(fly_event_manager_t *manager);

/* event setting */
fly_event_t *fly_event_init(fly_event_manager_t *manager);
int fly_event_register(fly_event_t *event);
int fly_event_unregister(fly_event_t *event);

/* event_timer */
int fly_event_timer_init(fly_event_t *event);
void fly_sec(fly_time_t *t, int sec);
void fly_msec(fly_time_t *t, int msec);
int is_fly_event_timeout(fly_event_t *e);
float fly_diff_time(struct timeval new, struct timeval old);
int fly_timeout_restart(fly_event_t *e);
void fly_event_error_add(fly_event_t *e, struct fly_err *__err);
void fly_jbhandle_setting(fly_event_manager_t *em, void (*handle)(fly_context_t *ctx));
struct fly_err;
enum fly_error_level;
struct fly_err *fly_event_err_init(fly_event_t *e, int __errno, enum fly_error_level level, const char *fmt, ...);


#define fly_event_err_init(ev, err_num, level, fmt, ...) \
			fly_err_init(		\
					(ev)->manager->pool,	\
					err_num,	\
					level,	\
					fmt,		\
					## __VA_ARGS__)

#endif
