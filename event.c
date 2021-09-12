#include "event.h"


fly_pool_t *fly_event_pool = NULL;
__fly_static fly_pool_t * __fly_event_pool_init(void);
__fly_static int __fly_event_fd_init(void);
__fly_static fly_event_t *__fly_nearest_event(fly_event_manager_t *manager);
__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager);
inline float fly_diff_time(fly_time_t new, fly_time_t old);
__fly_static void __fly_event_inherit_time(fly_event_t *dist, fly_event_t *src);
int fly_milli_time(fly_time_t t);
__fly_static int __fly_expired_event(fly_event_manager_t *manager);
__fly_static int __fly_event_handle(int epoll_events, fly_event_manager_t *manager);
__fly_static int __fly_event_handle_nomonitorable(fly_event_manager_t *manager);

__fly_static fly_pool_t *__fly_event_pool_init(void)
{
	if (fly_event_pool)
		return fly_event_pool;

	fly_event_pool = fly_create_pool(FLY_EVENT_POOL_SIZE);
	if (!fly_event_pool)
		return NULL;

	return fly_event_pool;
}

__fly_static int __fly_event_fd_init(void)
{
	int fd;
	fd = epoll_create1(EPOLL_CLOEXEC);
	if (fd == -1)
		return -1;

	return fd;
}

/*
 *	create manager of events.
 */
fly_event_manager_t *fly_event_manager_init(fly_context_t *ctx)
{
	fly_pool_t *pool;
	fly_event_manager_t *manager;
	int fd;

	if ((pool=__fly_event_pool_init()) == NULL)
		return NULL;

	manager = fly_pballoc(pool, sizeof(fly_event_manager_t));
	if (!manager)
		goto error;

	fd = __fly_event_fd_init();
	if (fd == -1)
		goto error;

	manager->pool = fly_event_pool;
	manager->evlist = fly_pballoc(manager->pool, sizeof(struct epoll_event)*FLY_EVLIST_ELES);
	manager->maxevents = FLY_EVLIST_ELES;
	manager->ctx = ctx;
	manager->efd = fd;
	manager->evlen = 0;
	manager->first = NULL;
	manager->last = NULL;

	return manager;
error:
	fly_delete_pool(&manager->pool);
	return NULL;
}

int fly_event_manager_release(fly_event_manager_t *manager)
{
	if (manager == NULL || manager->pool == NULL)
		return -1;

	if (close(manager->efd) == -1)
		return -1;
	return fly_delete_pool(&manager->pool);
}

fly_event_t *fly_event_init(fly_event_manager_t *manager)
{
	fly_event_t *event;

	if (manager == NULL || manager->pool == NULL)
		return NULL;

	event = fly_pballoc(manager->pool, sizeof(fly_event_t));
	if (!event)
		return NULL;

	event->manager = manager;
	fly_time_null(event->timeout);
	event->next = NULL;
	event->handler = NULL;
	return event;
}

__fly_static void __fly_event_inherit_time(fly_event_t *dist, fly_event_t *src)
{
	dist->timeout.tv_sec = src->timeout.tv_sec;
	dist->timeout.tv_usec = src->timeout.tv_usec;
	return;
}

int fly_event_register(fly_event_t *event)
{
	struct epoll_event ev;
	epoll_data_t data;
	int op;
	if (!event)
		return -1;

	op = EPOLL_CTL_ADD;
	if (event->manager->first != NULL){
		for (fly_event_t *e=event->manager->first; e!=NULL; e=e->next){
			if (e->fd == event->fd){
				/* if not same event */
				if (e != event){
					if (event->tflag & FLY_INHERIT)
						__fly_event_inherit_time(event, e);
					memcpy(e, event, sizeof(fly_event_t));
					/* TODO: release event. */
				}
				op = EPOLL_CTL_MOD;
				break;
			}
		}
	}

	if (op == EPOLL_CTL_ADD){
		if (event->manager->first == NULL)
			event->manager->first = event;
		else
			event->manager->last->next = event;
		event->manager->last = event;
		event->manager->evlen++;
	}
	data.ptr = event;
	ev.data = data;
	ev.events = event->read_or_write | event->eflag;

	if (fly_event_monitorable(event) && epoll_ctl(event->manager->efd, op, event->fd, &ev) == -1)
		return -1;

	return 0;
}

int fly_event_unregister(fly_event_t *event)
{
	fly_event_t *e, *prev;

	for (e=event->manager->first; e!=NULL; e=e->next){
		/* same fd event */
		if (event->fd == e->fd){
			if (event == event->manager->first && event == event->manager->last){
				event->manager->first = NULL;
				event->manager->last = NULL;
			}else if (e == event->manager->first){
				event->manager->first = e->next;
			}else if (e == event->manager->last){
				prev->next = NULL;
				event->manager->last = prev;
			}else
				prev->next = e->next;

			event->manager->evlen--;
			e->next = NULL;
			if (event->flag & FLY_CLOSE_EV || !fly_event_monitorable(event))
				return 0;
			else
				return epoll_ctl(event->manager->efd, EPOLL_CTL_DEL, event->fd, NULL);
		}
		prev = e;
	}
	return -1;
}

/*
 *		if t1>t2, return > 0
 *		if t1==t2, return 0
 *		if t1<t2, return < 0
 */

int fly_cmp_time(fly_time_t t1, fly_time_t t2)
{
	int delta_sec = (int) (t1).tv_sec - (int) (t2).tv_sec;
	if (delta_sec > 0)
		return 1;
	else if (delta_sec < 0)
		return -1;
	long delta_usec = (long) (t1).tv_usec - (long) (t2).tv_usec;
	if (delta_usec > 0)
		return 1;
	else if (delta_usec < 0)
		return -1;
	if (delta_sec == 0 && delta_usec == 0)
		return 0;

	return 0;
}

void fly_sec(fly_time_t *t, int sec)
{
	t->tv_sec = sec;
	t->tv_usec = 0;
	return;
}
void fly_msec(fly_time_t *t, int msec)
{
	t->tv_sec = (int) msec/1000;
	t->tv_usec = 1000*((long) msec%1000);
	return;
}

int fly_minus_time(fly_time_t t1)
{
	return (int) t1.tv_sec < 0;
}

__fly_static fly_event_t *__fly_nearest_event(fly_event_manager_t *manager)
{
	if (manager == NULL)
		return NULL;

	fly_event_t *near_timeout;
	near_timeout = NULL;
	if (manager->first == NULL)
		return NULL;
	for (fly_event_t *e=manager->first; e!=NULL; e=e->next){
		if (fly_not_infinity(e) && \
				( near_timeout == NULL || fly_cmp_time(near_timeout->timeout, e->timeout) < 0))
			near_timeout = e;
	}
	return near_timeout;
}

__fly_static int __fly_expired_event(fly_event_manager_t *manager)
{
	if (__fly_update_event_timeout(manager) == -1)
		return -1;

	if (manager->first == NULL)
		return 0;

	for (fly_event_t *e=manager->first; e!=NULL; e=e->next){
		if (e->expired)
			e->handler(e);
	}
	return 0;
}

void fly_sub_time(fly_time_t *t1, fly_time_t *t2)
{
	t1->tv_sec = (int) t1->tv_sec - (int) t2->tv_sec;
	t1->tv_usec = (long) t2->tv_usec - (long) t2->tv_usec;

	if (t1->tv_usec < 0){
		t1->tv_sec--;
		t1->tv_usec += 1000*1000;
	}
	return;
}

void fly_sub_time_from_sec(fly_time_t *t1, float delta)
{
	t1->tv_sec = (int) t1->tv_sec - (int) delta;
	t1->tv_usec = (int) t1->tv_usec - (1000*1000*(delta - (float)((int) delta)));

	if (t1->tv_usec < 0){
		t1->tv_sec--;
		t1->tv_usec += 1000*1000;
	}
	return; }

__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager)
{
	if (manager == NULL)
		return -1;

	static fly_time_t prev_time = FLY_TIME_NULL;
	fly_time_t now;
	float diff;

	if (is_fly_time_null(&prev_time) && fly_time(&prev_time) == -1)
		return -1;

	if (fly_time(&now) == -1)
		return -1;
	diff = fly_diff_time(now, prev_time);

	if (manager->first == NULL)
		return 0;

	for (fly_event_t *e=manager->first; e!=NULL; e=e->next)
		if (!(e->tflag & FLY_INFINITY)){
			fly_sub_time_from_sec(&e->timeout, diff);
			if (fly_minus_time(e->timeout))
				e->expired = true;
		}

	/* udpate prev time */
	if (fly_time(&prev_time) == -1)
		return -1;
	return 0;
}

int fly_milli_time(fly_time_t t)
{
	int msec = 0;
	msec += (int) 1000*((int) t.tv_sec);

	if (msec < 0)
		return msec;

	msec += (int) ((int) t.tv_usec/1000);
	return msec;
}

__fly_static int __fly_event_handle_nomonitorable(__unused fly_event_manager_t *manager)
{
	if (manager->first == NULL)
		return 0;

	for (fly_event_t *e=manager->first; e; e=e->next){
		if (fly_event_nomonitorable(e)){
			e->handler(e);
		}
	}
	return 0;
}

__fly_static int __fly_event_handle(int epoll_events, fly_event_manager_t *manager)
{
	struct epoll_event *event;
	for (int i=0; i<epoll_events; i++){
		fly_event_t *fly_event;
		event = manager->evlist + i;

		fly_event = (fly_event_t *) event->data.ptr;
		fly_event->available = true;
		/* TODO: handle */
		if (fly_event->handler)
			fly_event->handler(fly_event);

		/* remove event if not persistent */
		if (!fly_nodelete(fly_event)){
			if(fly_event_unregister(fly_event) == -1){
				return -1;
			}
		}
	}

	__fly_event_handle_nomonitorable(manager);

	return 0;
}

int fly_event_handler(fly_event_manager_t *manager)
{
	int epoll_events;
	fly_event_t *near_timeout;
	if (!manager || manager->efd < 0)
		return -1;

	for (;;){
		int timeout_msec;
		/* update event timeout */
		__fly_update_event_timeout(manager);
		near_timeout = __fly_nearest_event(manager);
		if (near_timeout != NULL){
			timeout_msec = fly_milli_time(near_timeout->timeout);
			if (timeout_msec < 0)
				timeout_msec = 0;
		}else
			timeout_msec = -1;

		/* the event with closest timeout */
		epoll_events = epoll_wait(manager->efd, manager->evlist, manager->maxevents, timeout_msec);
		switch(epoll_events){
		case 0:
			/* trigger expired event */
			if (__fly_expired_event(manager) == -1)
				return -1;

			break;
		case -1:
			/* epoll error */
			return -1;
		default:
			break;
		}

		__fly_event_handle(epoll_events, manager);
	}
}

inline int is_fly_event_timeout(fly_event_t *e)
{
	return e->expired & 1;
}

float fly_diff_time(fly_time_t new, fly_time_t old)
{
	int sec;
	long usec;
	sec = new.tv_sec - old.tv_sec;
	if (new.tv_usec == old.tv_usec)
		usec = 0.0;

	usec = new.tv_usec - old.tv_usec;
	while(usec < 0){
		sec--;
		usec += 1000*1000;
	}
	return (float) sec + (float) usec/((float) 1000*1000);
}
