#include "event.h"


fly_pool_t *fly_event_pool = NULL;
__fly_static fly_pool_t * __fly_event_pool_init(void);
__fly_static int __fly_event_fd_init(void);
__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager, fly_time_t delta);
__fly_static fly_event_t *__fly_nearest_event(fly_event_manager_t *manager);

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
				op = EPOLL_CTL_MOD;
				break;
			}
		}
	}

	if (event->manager->first == NULL)
		event->manager->first = event;
	else
		event->manager->last->next = event;

	event->manager->last = event;
	data.ptr = event;
	ev.data = data;
	ev.events = event->read_or_write | event->eflag;

	return epoll_ctl(event->manager->efd, op, event->fd, &ev);
}

int fly_event_unregister(fly_event_t *event)
{
	fly_event_t *e, *prev;

	for (e=event->manager->first; e!=NULL; e=e->next){
		if (event == e){
			if (event == event->manager->first && event == event->manager->last){
				event->manager->first = NULL;
				event->manager->last = NULL;
			}else if (e == event->manager->first)
				event->manager->first = e->next;
			else if (e == event->manager->last)
				event->manager->last = prev;
			else
				prev->next = e->next;

			return epoll_ctl(event->manager->efd, EPOLL_CTL_DEL, event->fd, NULL);
		}
		prev = e;
	}
	return 0;
}

/*
 *		if t1>t2, return > 0
 *		if t1==t2, return 0
 *		if t1<t2, return < 0
 */

int fly_cmp_time(struct timeval t1, struct timeval t2)
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

int fly_minus_time(struct timeval t1)
{
	return (int) t1.tv_sec < 0;
}

__fly_static fly_event_t *__fly_nearest_event(fly_event_manager_t *manager)
{
	if (manager == NULL)
		return NULL;

	fly_event_t *near_timeout;
	if (manager->first == NULL)
		return NULL;
	for (fly_event_t *e=manager->first; e!=NULL; e=e->next){
		if (fly_cmp_time(near_timeout->timeout, e->timeout) < 0)
			near_timeout = e;
	}
	return near_timeout;
}

__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager, fly_time_t delta)
{
	if (manager == NULL)
		return -1;

	fly_time_t now;
	float diff;

	if (fly_time(&now) == -1)
		return -1;
	diff = fly_diff_time(now, delta);

	if (manager->first == NULL)
		return 0;

	for (fly_event_t *e=manager->first; e!=NULL; e=e->next)
		if (!(e->tflag & FLY_INFINITY)){
			fly_sub_time(&e->timeout, &diff);
			if (fly_minus_time(e->timeout))
				e->expired = true;
		}

	return 0;
}

int fly_event_handler(fly_event_manager_t *manager)
{
	int epoll_events;
	fly_event_t *near_timeout;
	fly_time_t delta;
	struct epoll_event *event;
	if (!manager || manager->efd < 0)
		return -1;

	if (fly_time(&delta) == -1)
		return -1;

	for (;;){
		/* update event timeout */
		__fly_update_event_timeout(manager, delta);
		/* the event with closest timeout */
		near_timeout = __fly_nearest_event(manager);
		epoll_events = epoll_wait(manager->efd, manager->evlist, manager->maxevents, near_timeout != NULL ? fly_milli_time(near_timeout->timeout) : -1);
		switch(epoll_events){
		case 0:
			/* timeout */
			break;
		case -1:
			/* epoll error */
			return -1;
		default:
			break;
		}

		for (int i=0; i<epoll_events; i++){
			fly_event_t *fly_event;
			event = manager->evlist + i;

			fly_event = (fly_event_t *) event->data.ptr;
			fly_event->available = true;
			/*TODO: handle*/
			if (fly_event->handler)
				fly_event->handler(fly_event);

			/* remove event if not persistent */
			if (!fly_nodelete(fly_event)){
				if(fly_event_unregister(fly_event) == -1){
					return -1;
				}
			}
		}
	}
}
