#ifndef _EVENT_H
#define _EVENT_H


#include <sys/epoll.h>
#include <sys/timerfd.h>
#include "alloc.h"

extern fly_pool_t *fly_event_pool;
#define FLY_EVENT_POOL_SIZE			100
#define FLY_READ		EPOLLIN
#define FLY_WRITE		EPOLLOUT
#define FLY_EVLIST_ELES			1000
struct fly_event_manager{
	fly_pool_t *pool;
	int efd;
	struct epoll_event *evlist;
	int maxevents;

	struct fly_event *first;
	struct fly_event *last;
};
typedef struct fly_event_manager fly_event_manager_t;

struct fly_event{
	fly_event_manager_t *manager;
	int fd;
	int read_or_write;
	int eflag;

	int timerfd;
	struct itimerspec *time;
	int tflag;

	int flag;
	struct fly_event *next;

	int (*handler)(struct fly_event *);

	void *event_data;
};
typedef struct fly_event fly_event_t;

/* eflag(epoll flag) shown detail in epoll API */
/* tflag(time flag) */
#define FLY_TIMEOUT		1<<0
#define FLY_WAITFIRST	1<<1
/* flag(generary flag) */
#define FLY_PERSISTENT	1<<0
#define FLY_NODELETE	1<<1
#define FLY_TIMER_NOW	1<<2
#define fly_nodelete(e)						\
	(										\
		((e)->flag & FLY_PERSISTENT)	||	\
		((e)->flag & FLY_NODELETE)			\
	)

/* manager setting */
fly_event_manager_t *fly_event_manager_init(void);
int fly_event_manager_release(fly_event_manager_t *manager);
int fly_event_handler(fly_event_manager_t *manager);

/* event setting */
fly_event_t *fly_event_init(fly_event_manager_t *manager);
int fly_event_register(fly_event_t *event);
int fly_event_unregister(fly_event_t *event);

/* event_timer */
int fly_event_timer_init(fly_event_t *event);
#endif
