#include "event.h"
#include "err.h"


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
__fly_static int __fly_event_handle_failure_log(fly_event_t *e);
__fly_static int __fly_event_cmp(void *k1, void *k2);
static void fly_event_handle(fly_event_t *e);

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
	if (fly_unlikely_null(manager->evlist))
		return NULL;
	fly_queue_init(&manager->unmonitorable);
	manager->maxevents = FLY_EVLIST_ELES;
	manager->ctx = ctx;
	manager->efd = fd;
	manager->evlen = 0;
	FLY_MANAGER_DUMMY_INIT(manager);
	manager->first = manager->dummy;
	manager->last = manager->dummy;
	manager->rbtree = fly_rb_tree_init(__fly_event_cmp);
	if (fly_unlikely_null(manager->rbtree))
		return NULL;

	return manager;
error:
	fly_delete_pool(&manager->pool);
	return NULL;
}

__fly_static int __fly_event_cmp(void *k1, void *k2)
{
	fly_time_t *t1 = (fly_time_t *) k1;
	fly_time_t *t2 = (fly_time_t *) k2;

	if (t1->tv_sec > t2->tv_sec)
		return FLY_RB_CMP_BIG;
	else if (t1->tv_sec < t2->tv_sec)
		return FLY_RB_CMP_SMALL;
	else{
		if (t1->tv_usec > t2->tv_usec)
			return FLY_RB_CMP_BIG;
		else if (t1->tv_usec < t2->tv_usec)
			return FLY_RB_CMP_SMALL;
		else
			return FLY_RB_CMP_EQUAL;
	}
	FLY_NOT_COME_HERE;
}

int fly_event_manager_release(fly_event_manager_t *manager)
{
	if (manager == NULL || manager->pool == NULL)
		return -1;

	if (close(manager->efd) == -1)
		return -1;

	fly_rb_tree_release(manager->rbtree);
	fly_delete_pool(&manager->pool);
	return 0;
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
	event->rbnode = NULL;
	event->fd = -1;
	fly_time_null(event->timeout);
	fly_time_zero(event->abs_timeout);
	fly_time_zero(event->start);
	event->next = manager->dummy;
	event->prev = manager->dummy;
	event->handler = NULL;
	event->handler_name = NULL;
	event->fail_close = NULL;
	if (fly_time(&event->spawn_time) == -1)
		return NULL;
	fly_queue_init(&event->qelem);
	event->expired = false;
	event->available = false;
	event->available_row = 0;
	event->rbnode = NULL;
	return event;
}

__fly_static void __fly_event_inherit_time(fly_event_t *dist, fly_event_t *src)
{
	dist->timeout.tv_sec = src->timeout.tv_sec;
	dist->timeout.tv_usec = src->timeout.tv_usec;
	return;
}

static inline void __fly_add_time_from_now(fly_time_t *t1, fly_time_t *t2)
{
	if (fly_time(t1) == -1)
		return;

	t1->tv_sec = t1->tv_sec + t2->tv_sec;
	t1->tv_usec = t1->tv_usec + t2->tv_usec;
	if (t1->tv_usec >= 1000000){
		t1->tv_sec++;
		t1->tv_usec -= 1000000;
	}
}

int fly_event_register(fly_event_t *event)
{
	struct epoll_event ev;
	epoll_data_t data;
	int op;

	if (fly_unlikely_null(event))
		return -1;

	op = EPOLL_CTL_ADD;
	/* if exist some event. */
	if (event->manager->first != event->manager->dummy){
		for (fly_event_t *e=event->manager->dummy->next; e!=event->manager->dummy; e=e->next){
			if (e->fd == event->fd){
				/* if not same event */
				if (e != event){
					if (event->tflag & FLY_INHERIT)
						__fly_event_inherit_time(event, e);
					memcpy(e, event, sizeof(fly_event_t));
					/* release event. */
					fly_pbfree(event->manager->pool, event);
				}
				op = EPOLL_CTL_MOD;
				break;
			}
		}
	}

	if (op == EPOLL_CTL_ADD){
		/* add to red black tree */
		if (!(event->tflag & FLY_INFINITY) && fly_event_monitorable(event)){
			__fly_add_time_from_now(&event->abs_timeout, &event->timeout);
			event->rbnode = fly_rb_tree_insert(event->manager->rbtree, event, &event->abs_timeout);
		}

		if (fly_event_monitorable(event)){
			if (event->manager->first == event->manager->dummy){
				event->manager->first = event;
				event->manager->dummy->next = event->manager->first;
			}else{
				event->manager->last->next = event;
				event->prev = event->manager->last;
			}
			event->manager->dummy->prev = event;
			event->next = event->manager->dummy;
			event->manager->last = event;
		}else{
			fly_queue_push(&event->manager->unmonitorable, &event->qelem);
		}
		event->manager->evlen++;
	}else{
		/* delete & add (for changing timeout) */
		if (fly_event_monitorable(event)){
			if (event->rbnode)
				fly_rb_delete(event->manager->rbtree, event->rbnode);
			if (!(event->tflag & FLY_INFINITY) && !(event->tflag & FLY_INHERIT))
					__fly_add_time_from_now(&event->abs_timeout, &event->timeout);
			if (!(event->tflag & FLY_INFINITY) && fly_event_monitorable(event))
				event->rbnode = fly_rb_tree_insert(event->manager->rbtree, event, &event->abs_timeout);
		}
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

	if (fly_event_nomonitorable(event)){
		fly_queue_remove(&event->qelem);
		if (fly_is_queue_empty(&event->manager->unmonitorable))
			fly_queue_empty(&event->manager->unmonitorable);
		fly_pbfree(event->manager->pool, event);
		return 0;
	}else{
		for (e=event->manager->dummy->next; e!=event->manager->dummy; e=e->next){
			/* same fd event */
			if (event->fd == e->fd){
				int efd;

				/* delete from red black tree */
				if (!(event->tflag & FLY_INFINITY) && fly_event_monitorable(event))
					fly_rb_delete(event->manager->rbtree, event->rbnode);
				/* only one event */
				if (event == event->manager->first && event == event->manager->last){
					event->manager->first = event->manager->dummy;
					event->manager->last = event->manager->dummy;
					event->manager->dummy->next = event->manager->dummy;
					/* first event */
				}else if (e == event->manager->first){
					event->manager->first = e->next;
					event->manager->dummy->next = event->manager->first;
					/* last event */
				}else if (e == event->manager->last){
					prev->next = e->next;
					event->manager->last = prev;
				}else
					prev->next = e->next;

				event->manager->evlen--;
				efd = event->manager->efd;

				/* release event */
				if (event->flag & FLY_CLOSE_EV){
					fly_pbfree(event->manager->pool, event);
					return 0;
				}else{
					fly_pbfree(event->manager->pool, event);
					return epoll_ctl(efd, EPOLL_CTL_DEL, event->fd, NULL);
				}
			}
			prev = e;
		}
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

int fly_timeout_restart(fly_event_t *e)
{
	if (fly_time(&e->start) == -1)
		return -1;

	__fly_add_time_from_now(&e->abs_timeout, &e->timeout);
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
	fly_rb_node_t *node;

	if (!manager->rbtree->node_count)
		return NULL;

	node = manager->rbtree->root->node;
	while(node->c_left != nil_node_ptr)
		node = node->c_left;

	return (fly_event_t *) node->data;
}

__fly_static int __fly_expired_event(fly_event_manager_t *manager)
{
	if (__fly_update_event_timeout(manager) == -1)
		return -1;

	if (manager->first == NULL)
		return 0;

	for (fly_event_t *e=manager->dummy->next; e!=manager->dummy; e=e->next){
		if (e->expired)
			fly_event_handle(e);
	}
	return 0;
}

void fly_sub_time(fly_time_t *t1, fly_time_t *t2)
{
	t1->tv_sec = (int) t1->tv_sec - (int) t2->tv_sec;
	t1->tv_usec = (long) t1->tv_usec - (long) t2->tv_usec;

	if (t1->tv_usec < 0){
		t1->tv_sec--;
		t1->tv_usec += 1000*1000;
	}
	return;
}

void fly_sub_time_from_base(fly_time_t *dist, fly_time_t *__sub, fly_time_t *base)
{
	dist->tv_sec = (int) __sub->tv_sec - (int) base->tv_sec;
	dist->tv_usec = (long) __sub->tv_usec - (long) base->tv_usec;

	if (dist->tv_usec < 0){
		dist->tv_sec--;
		dist->tv_usec += 1000*1000;
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
	return;
}

__fly_static void __fly_reconnect_event_by_expired(fly_event_t *event, fly_event_manager_t *manager)
{
	if (!event->expired)
		return;

	if (manager->first == event)
		return;
	else{
		event->prev->next = event->next;
		event->next->prev = event->prev;

		event->prev = manager->dummy;
		event->next = manager->dummy->next;
		manager->dummy->next->prev = event;
		manager->dummy->next = event;
		manager->first = event;
	}
	return;
}

__fly_static int __fly_expired_from_rbtree(fly_event_manager_t *manager, fly_rb_tree_t *tree, fly_rb_node_t *node, fly_time_t *__t)
{
	fly_event_t *__e;
	if (node == nil_node_ptr)
		return 0;

	while(node!=nil_node_ptr){
		switch(tree->cmp(node->key, __t)){
		case FLY_RB_CMP_BIG:
			node = node->c_left;
			break;
		case FLY_RB_CMP_SMALL:
		case FLY_RB_CMP_EQUAL:
			/* __n right partial tree is expired */
			__e = (fly_event_t *) node->data;
			__e->expired = true;
			__fly_expired_from_rbtree(manager, tree, node->c_left, __t);
			__fly_expired_from_rbtree(manager, tree, node->c_right, __t);

			__fly_reconnect_event_by_expired(__e, manager);
			return 0;
		default:
			FLY_NOT_COME_HERE
		}
	}
	return 0;
}

__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager)
{
	if (manager == NULL)
		return -1;

	fly_time_t now;
	if (fly_time(&now) == -1)
		return -1;
	if (manager->first == NULL)
		return 0;

	if (manager->rbtree->node_count == 0)
		return 0;
	else{
		struct fly_rb_tree *tree;
		struct fly_rb_node *__n;

		tree = manager->rbtree;
		__n = tree->root->node;

		return __fly_expired_from_rbtree(manager, tree, __n, &now);
	}
}

int fly_milli_diff_time_from_now(fly_time_t *t)
{
	int res;
	fly_time_t now;

	if (fly_time(&now) == -1)
		return -1;

	res = 1000*(t->tv_sec-now.tv_sec);
	res += (t->tv_usec-now.tv_usec)/1000;

	return res;
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
	if (manager->unmonitorable.count == 0)
		return 0;

	struct fly_queue *__q;
	for (__q=manager->unmonitorable.next; manager->unmonitorable.count>0; __q=fly_queue_pop(&manager->unmonitorable))
		fly_event_handle(fly_queue_data(__q, struct fly_event, qelem));

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
		fly_event->available_row = event->events;
		fly_event_handle(fly_event);

		/* remove event if not persistent */
		if (fly_unlikely_null(fly_event) && \
				!fly_nodelete(fly_event) && \
				(fly_event_unregister(fly_event) == -1))
			return -1;
	}

	return __fly_event_handle_nomonitorable(manager);
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
		if (near_timeout){
			timeout_msec = fly_milli_diff_time_from_now(&near_timeout->abs_timeout);
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

#include "log.h"
__fly_static int __fly_event_handler_failure_logcontent(fly_logcont_t *lc, fly_event_t *e)
{
#define __FLY_EVENT_HANDLER_FAILURE_LOGCONTENT_SUCCESS	1
#define __FLY_EVENT_HANDLER_FAILURE_LOGCONTENT_ERROR	-1
#define __FLY_EVENT_HANDLER_FAILURE_LOGCONTENT_OVERFLOW	0
	int res;
	res = snprintf(
		(char *) lc->content,
		(size_t) lc->contlen,
		"event fd: %d. handler: %s",
		e->fd,
		e->handler_name!=NULL ? e->handler_name : "?"
	);

	if (res >= (int) fly_maxlog_length(lc->contlen)){
		memcpy(fly_maxlog_suffix_point(lc->content,lc->contlen), FLY_LOGMAX_SUFFIX, strlen(FLY_LOGMAX_SUFFIX));
		return __FLY_EVENT_HANDLER_FAILURE_LOGCONTENT_OVERFLOW;
	}
	lc->contlen = res;
	return __FLY_EVENT_HANDLER_FAILURE_LOGCONTENT_SUCCESS;
}

__fly_static int __fly_event_handle_failure_log(fly_event_t *e)
{
	fly_logcont_t *lc;

	lc = fly_logcont_init(fly_log_from_event(e), FLY_LOG_NOTICE);
	if (lc == NULL)
		return -1;

	if (fly_logcont_setting(lc, FLY_EVENT_HANDLE_FAILURE_LOG_MAXLEN) == -1)
		return -1;

	if (__fly_event_handler_failure_logcontent(lc, e) == -1)
		return -1;

	if (fly_log_now(&lc->when) == -1)
		return -1;

	/* close failure fd*/
	if ((e->fail_close != NULL ? e->fail_close(e->fd) : close(e->fd)) == -1)
		return -1;

	FLY_EVENT_HANDLER(e, fly_log_event_handler);
	e->fd = fly_log_from_event(e)->notice->file;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = 0;
	e->eflag = 0;
	e->available = false;
	e->expired = false;
	e->event_data = (void *) lc;
	fly_time_zero(e->timeout);
	fly_event_regular(e);

	return fly_event_register(e);
}

int fly_event_inherit_register(fly_event_t *e)
{
	e->tflag |= FLY_INHERIT;
	e->flag |= FLY_MODIFY;
	return fly_event_register(e);
}

static void fly_event_handle(fly_event_t *e)
{
	int handle_result;
	if (e->handler != NULL)
		handle_result = e->handler(e);
	if (handle_result == FLY_EVENT_HANDLE_FAILURE)
		/* log error handle in notice log. */
		if (__fly_event_handle_failure_log(e) == -1)
			FLY_EMERGENCY_ERROR(
				FLY_EMERGENCY_STATUS_ELOG,
				"failure to log event handler failure."
			);
	return;
}
