#ifndef _EVENT_H
#define _EVENT_H


#include <sys/epoll.h>
#include <sys/timerfd.h>
#include <sys/time.h>
#include "alloc.h"
#include "context.h"

extern fly_pool_t *fly_event_pool;
#define FLY_EVENT_POOL_SIZE			100
#define FLY_READ		EPOLLIN
#define FLY_WRITE		EPOLLOUT
#define FLY_EVLIST_ELES			1000
struct fly_event_manager{
	fly_pool_t *pool;
	fly_context_t *ctx;
	int efd;
	struct epoll_event *evlist;
	int maxevents;

	struct fly_event *first;
	struct fly_event *last;
};
typedef struct fly_event_manager fly_event_manager_t;

typedef struct timeval fly_time_t;
struct fly_event{
	fly_event_manager_t *manager;
	int fd;
	int read_or_write;
	int eflag;

	fly_time_t timeout;
	int tflag;

	int flag;
	struct fly_event *next;

	int (*handler)(struct fly_event *);

	void *event_data;
	void *event_state;

	/* event bit fields */
	int expired: 1;
	int available: 1;
};
typedef struct fly_event fly_event_t;
#define fly_time(tptr)		gettimeofday((struct timeval *) tptr, NULL)

#define fly_time_null(t)									\
	do {														\
		(t).tv_sec = -1;									\
		(t).tv_usec = -1;									\
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
#define fly_milli_time(t1)								\
	({													\
		int milli = 0;									\
		milli += (int) 1000*((int) (t1).tv_sec);		\
		milli += (int) ((int) (t1).tv_usec) / 1000;		\
	})

/* eflag(epoll flag) shown detail in epoll API */
/* tflag(time flag) */
#define FLY_INFINITY	1<<0
#define FLY_TIMEOUT		1<<1
#define FLY_WAITFIRST	1<<2
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
#endif
