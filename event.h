#ifndef _EVENT_H
#define _EVENT_H

#include <stdio.h>
#include <stdbool.h>
#include <sys/epoll.h>
#include <sys/signalfd.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include "alloc.h"
#include "util.h"
#include  "rbtree.h"

extern fly_pool_t *fly_event_pool;
#define FLY_EVENT_POOL_SIZE			100
#define FLY_READ		EPOLLIN
#define FLY_WRITE		EPOLLOUT
#define FLY_EVLIST_ELES			1000

typedef struct fly_context fly_context_t;
struct fly_event_manager{
	fly_pool_t *pool;
	fly_context_t *ctx;
	int efd;
	struct epoll_event *evlist;
	int maxevents;
	int evlen;

	struct fly_rb_tree *rbtree;
	struct fly_event *first;
	struct fly_event *last;
	struct fly_event *dummy;
};
typedef struct fly_event_manager fly_event_manager_t;
#define FLY_MANAGER_DUMMY_INIT(m)			\
	do{										\
		(m)->dummy = fly_pballoc((m)->pool, sizeof(struct fly_event)); \
		memset((m)->dummy, 0, sizeof(struct fly_event_manager));			\
		(m)->dummy->next = (m)->dummy;				\
		(m)->dummy->prev = (m)->dummy;				\
		FLY_EVENT_HANDLER((m)->dummy, NULL);		\
	} while(0);

typedef struct timeval fly_time_t;
struct fly_event{
	fly_event_manager_t *manager;
	int fd;
	int read_or_write;
	int available_row;
	int eflag;

	fly_time_t timeout;
	fly_time_t spawn_time;
	fly_time_t start;
	fly_time_t abs_timeout;
	int tflag;

	int flag;
	struct fly_event *next;
	struct fly_event *prev;

	struct fly_rb_node *rbnode;

	int (*handler)(struct fly_event *);
	char *handler_name;

	void *event_data;
	void *event_fase;
	void *event_state;

	/*
	 * if event handler fail, this function is called.
	 * this function must close fd(event file).
	 */
	int (*fail_close)(int fd);

	/* event bit fields */
	fly_bit_t file_type: 4;
	fly_bit_t expired: 1;
	fly_bit_t available: 1;
};

#define FLY_EVENT_HANDLER(e, __handler)	\
	do{									\
		(e)->handler = (__handler);		\
		(e)->handler_name = #__handler;	\
	} while(0);

#define FLY_HANDLE_EVENT(__event)									\
	do{																\
		int handle_result;											\
		if ((__event)->handler != NULL)								\
			handle_result = (__event)->handler(__event);			\
		if (handle_result == FLY_EVENT_HANDLE_FAILURE)				\
			/* log error handle in notice log. */					\
			if (__fly_event_handle_failure_log((__event)) == -1)	\
				FLY_EMERGENCY_ERROR(								\
					FLY_EMERGENCY_STATUS_ELOG,						\
					"failure to log event handler failure."			\
				);													\
	}while(0)

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
		((e)->flag & FLY_MODIFY)		||	\
		((e)->flag & FLY_CLOSE_EV)			\
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
#define fly_evnet_is_inotify(e)		fly_event_is_file_type((e), INOTIFY)

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
#define fly_event_nomonitorable(e)	(!(fly_event_monitorable((e))))

#define FLY_EVENT_HANDLE_FAILURE	(-1)
#define FLY_EVENT_HANDLE_FAILURE_LOG_MAXLEN		100

/* manager setting */
fly_event_manager_t *fly_event_manager_init(fly_context_t *ctx);
int fly_event_manager_release(fly_event_manager_t *manager);
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
#endif
