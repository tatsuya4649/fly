#include "event.h"


fly_pool_t *fly_event_pool = NULL;
__fly_static fly_pool_t * __fly_event_pool_init(void);
__fly_static int __fly_event_fd_init(void);

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
fly_event_manager_t *fly_event_manager_init(void)
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
	manager->fd = fd;
	manager->first = NULL;

	return manager;
error:
	fly_delete_pool(&manager->pool);
	return NULL;
}

int fly_event_manager_release(fly_event_manager_t *manager)
{
	if (manager == NULL || manager->pool == NULL)
		return -1;

	if (close(manager->fd) == -1)
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
	event->timerfd = -1;
	event->time = NULL;
	event->next = NULL;
	event->handler = NULL;
	return event;
}

int fly_event_register(fly_event_t *event)
{
	struct epoll_event ev;
	epoll_data_t data;
	if (!event)
		return -1;

	if (event->manager->last == NULL)
		event->manager->last = event;
	else
		event->manager->last->next = event;

	data.ptr = event;
	ev.data = data;
	ev.events = event->read_or_write | event->eflag;
	return epoll_ctl(event->manager->fd, EPOLL_CTL_ADD, event->fd, &ev);
}

int fly_event_unregister(fly_event_t *event)
{
	fly_event_t *e, *prev;

	for (e=event->manager->first; e!=NULL; e=e->next){
		if (event == e){
			if (e == event->manager->first)
				event->manager->first = e->next;
			else
				prev->next = e->next;
		}
		prev = e;
	}
	return -1;
}

int fly_event_timer_init(fly_event_t *event)
{
	int timerfd;

	timerfd = timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC);
	if (timerfd == -1)
		return -1;

	event->timerfd = timerfd;
	return 0;
}


int fly_event_handler(fly_event_manager_t *manager)
{
	int epoll_events;
	struct epoll_event *event;
	if (!manager || manager->fd < 0)
		return -1;

	for (;;){
		epoll_events = epoll_wait(manager->fd, manager->evlist, manager->maxevents, -1);
		if (epoll_events == -1)
			return -1;

		for (int i=0; i<epoll_events; i++){
			fly_event_t *fly_event;
			event = manager->evlist + i;

			fly_event = (fly_event_t *) event->data.ptr;
			/*TODO: handle*/
			if (fly_event->handler)
				fly_event->handler(fly_event->fd);

			/* remove event if not persistent */
			if (!(fly_event->flag & FLY_PERSISTENT)){
				if(epoll_ctl(manager->fd, EPOLL_CTL_DEL, fly_event->fd, NULL) == -1)
					return -1;
			}
		}
	}
}
