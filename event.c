#include "event.h"
#include "err.h"


fly_pool_t *fly_event_pool = NULL;
__fly_static fly_pool_t *__fly_event_pool_init(fly_context_t *ctx);
__fly_static int __fly_event_fd_init(void);
__fly_static fly_event_t *__fly_nearest_event(fly_event_manager_t *manager);
__fly_static int __fly_update_event_timeout(fly_event_manager_t *manager);
inline float fly_diff_time(fly_time_t new, fly_time_t old);
int fly_milli_time(fly_time_t t);
__fly_static int __fly_expired_event(fly_event_manager_t *manager);
__fly_static int __fly_event_handle(int epoll_events, fly_event_manager_t *manager);
__fly_static int __fly_event_handle_nomonitorable(fly_event_manager_t *manager);
__fly_static int __fly_event_handle_failure_log(fly_event_t *e);
__fly_static int __fly_event_cmp(void *k1, void *k2, void *);
static void fly_event_handle(fly_event_t *e);

__fly_static fly_pool_t *__fly_event_pool_init(fly_context_t *ctx)
{
	if (fly_event_pool)
		return fly_event_pool;

	fly_event_pool = fly_create_pool(ctx->pool_manager, FLY_EVENT_POOL_SIZE);
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

	if ((pool=__fly_event_pool_init(ctx)) == NULL)
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
	fly_queue_init(&manager->monitorable);
	fly_queue_init(&manager->unmonitorable);
	manager->maxevents = FLY_EVLIST_ELES;
	manager->ctx = ctx;
	manager->efd = fd;
	manager->rbtree = fly_rb_tree_init(__fly_event_cmp);
	if (fly_unlikely_null(manager->rbtree))
		return NULL;

	return manager;
error:
	fly_delete_pool(manager->pool);
	return NULL;
}

__fly_static int __fly_event_cmp(void *k1, void *k2, void *)
{
	struct __fly_event_for_rbtree *__e1, *__e2;
	__e1 = (struct __fly_event_for_rbtree *) k1;
	__e2 = (struct __fly_event_for_rbtree *) k2;

#ifdef DEBUG
	assert(__e1 != NULL);
	assert(__e2 != NULL);
	assert(__e1->ptr != NULL);
	assert(__e2->ptr != NULL);
	assert(__e1->abs_timeout != NULL);
	assert(__e2->abs_timeout != NULL);
#endif

	if (__e1->abs_timeout->tv_sec > __e2->abs_timeout->tv_sec)
		return FLY_RB_CMP_BIG;
	else if (__e1->abs_timeout->tv_sec < __e2->abs_timeout->tv_sec)
		return FLY_RB_CMP_SMALL;
	else{
		if (__e1->abs_timeout->tv_usec > __e2->abs_timeout->tv_usec)
			return FLY_RB_CMP_BIG;
		else if (__e1->abs_timeout->tv_usec < __e2->abs_timeout->tv_usec)
			return FLY_RB_CMP_SMALL;
		else{
			if (__e1 > __e2)
				return FLY_RB_CMP_BIG;
			else if (__e1 < __e2)
				return FLY_RB_CMP_SMALL;
			else
				return FLY_RB_CMP_EQUAL;
		}
		FLY_NOT_COME_HERE;
	}
	FLY_NOT_COME_HERE;
}

/* event end handle */
int fly_event_manager_release(fly_event_manager_t *manager)
{
	if (manager == NULL || manager->pool == NULL)
		return -1;

	if (close(manager->efd) == -1)
		return -1;

	struct fly_queue *__b;
	fly_event_t *__e;
	fly_for_each_queue(__b, &manager->monitorable){
		__e = fly_bllist_data(__b, fly_event_t, qelem);
		if (__e->end_handler)
			__e->end_handler(__e);
	}
	fly_for_each_queue(__b, &manager->unmonitorable){
		__e = fly_bllist_data(__b, fly_event_t, qelem);
		if (__e->end_handler)
			__e->end_handler(__e);
	}

	fly_rb_tree_release(manager->rbtree);
	fly_delete_pool(manager->pool);
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
	event->handler = NULL;
	event->end_handler = NULL;
	event->handler_name = NULL;
	event->fail_close = NULL;
	if (fly_time(&event->spawn_time) == -1)
		return NULL;
	fly_queue_init(&event->qelem);
	fly_queue_init(&event->uqelem);
	event->expired = false;
	event->expired_handler = NULL;
	event->expired_event_data = NULL;
	event->available = false;
	event->available_row = 0;
	event->rbnode = NULL;
	event->yetadd = true;
	event->end_event_data = NULL;

	FLY_EVENT_FOR_RBTREE_INIT(event);
	return event;
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

#ifdef DEBUG
void fly_event_debug_rbtree_delete_node(fly_event_manager_t *manager, fly_event_t *target)
{
	struct fly_queue *__q;
	fly_event_t *__e;

	fly_for_each_queue(__q, &manager->monitorable){
		__e = (fly_event_t *) fly_queue_data(__q, fly_event_t, qelem);
		if ((__e->flag & FLY_INFINITY))
			continue;

		if (__e == target){
			assert(__e->rbnode == NULL);
		}else{
			assert(__e->rbnode->data == __e);
			assert((*__e->rbnode->node_data)->data == __e);
		}
	}
}
void fly_event_debug_rbtree(fly_event_manager_t *manager)
{
	struct fly_queue *__q;
	fly_event_t *__e;

	fly_for_each_queue(__q, &manager->monitorable){
		__e = (fly_event_t *) fly_queue_data(__q, fly_event_t, qelem);
		if ((__e->flag & FLY_INFINITY))
			continue;

		assert(__e->rbnode->data == __e);
		assert((*__e->rbnode->node_data)->data == __e);
	}
}
#endif

int fly_event_register(fly_event_t *event)
{
	struct epoll_event ev;
	int op;
	epoll_data_t data;

	if (fly_unlikely_null(event))
		return -1;

#ifdef DEBUG
	fly_event_debug_rbtree(event->manager);
#endif
	op = fly_event_op(event);
	if (op == EPOLL_CTL_ADD){
		/* add to red black tree */
		if (!(event->tflag & FLY_INFINITY) && fly_event_monitorable(event)){
			__fly_add_time_from_now(&event->abs_timeout, &event->timeout);
			event->rbnode = fly_rb_tree_insert(event->manager->rbtree, event, &event->rbtree_elem, &event->rbnode, NULL);
#ifdef DEBUG
			int ret;

			ret = (event->rbnode == (*event->rbnode->node_data));
			assert(ret);
			ret = (event == (*event->rbnode->node_data)->data);
			assert(ret);
			ret = (event->rbnode->key-event->rbnode->data) == offsetof(fly_event_t, rbtree_elem);
			assert(ret);
			fly_event_debug_rbtree(event->manager);
#endif
		}

		if (fly_event_monitorable(event))
			fly_queue_push(&event->manager->monitorable, &event->qelem);
		else
			fly_queue_push(&event->manager->unmonitorable, &event->uqelem);
#ifdef DEBUG
		fly_event_debug_rbtree(event->manager);
#endif
		event->yetadd = false;
	}else{
		/* delete & add (for changing timeout) */
		if (fly_event_monitorable(event)){
			if (event->rbnode){
#ifdef DEBUG
				int ret;

				ret = (event->rbnode == (*event->rbnode->node_data));
				assert(ret);
				ret = event == (*event->rbnode->node_data)->data;
				assert(ret);
				ret = (event->rbnode->key-event->rbnode->data) == offsetof(fly_event_t, rbtree_elem);
				assert(ret);
				fly_event_debug_rbtree(event->manager);
#endif
				fly_rb_delete(event->manager->rbtree, event->rbnode);
#ifdef DEBUG
				//fly_queue_remove(&event->qelem);
				fly_event_debug_rbtree_delete_node(event->manager, event);
#endif
			}
			if (!(event->tflag & FLY_INFINITY) && !(event->tflag & FLY_INHERIT))
					__fly_add_time_from_now(&event->abs_timeout, &event->timeout);
			if (!(event->tflag & FLY_INFINITY) && fly_event_monitorable(event)){
				event->rbnode = fly_rb_tree_insert(event->manager->rbtree, event, &event->rbtree_elem, &event->rbnode, NULL);
#ifdef DEBUG
				//fly_queue_push(&event->manager->monitorable, &event->qelem);
				int ret;
				ret = (event->rbnode == (*event->rbnode->node_data));
				assert(ret);
				ret = (event == (*event->rbnode->node_data)->data);
				assert(ret);
				ret = (event->rbnode->key-event->rbnode->data) == offsetof(fly_event_t, rbtree_elem);
				assert(ret);
				fly_event_debug_rbtree(event->manager);
#endif
			}
		}
	}
	data.ptr = event;
	ev.data = data;
	ev.events = event->read_or_write | event->eflag;

#ifdef DEBUG
	if (!(event->tflag&FLY_INFINITY) && \
			fly_event_monitorable(event))
		assert(event->expired_handler != NULL);
#endif

	if (fly_event_monitorable(event) && epoll_ctl(event->manager->efd, op, event->fd, &ev) == -1)
		return -1;

	return 0;
}

int fly_event_unregister(fly_event_t *event)
{
#ifdef DEBUG
	fly_event_manager_t *__m=event->manager;
	fly_event_debug_rbtree(__m);
#endif
	if (fly_event_unmonitorable(event)){
		fly_queue_remove(&event->uqelem);
		fly_pbfree(event->manager->pool, event);
		return 0;
	}else{
		fly_queue_remove(&event->qelem);
		if (event->rbnode)
			fly_rb_delete(event->manager->rbtree, event->rbnode);
		if (!(event->flag & FLY_CLOSE_EV))
			if (epoll_ctl(event->manager->efd, EPOLL_CTL_DEL, event->fd, NULL) == -1)
				return -1;

		fly_pbfree(event->manager->pool, event);
		return 0;
	}
#ifdef DEBUG
	fly_event_debug_rbtree(__m);
#endif
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
	struct fly_queue *__q, *__n;
	fly_event_t *e;

	for (__q=manager->monitorable.next; __q!=&manager->monitorable; __q=__n){
		__n = __q->next;
		e = fly_queue_data(__q, fly_event_t, qelem);
		if (e->expired && e->expired_handler){
			if (e->expired_handler(e) == -1)
				FLY_EMERGENCY_ERROR(
					FLY_EMERGENCY_STATUS_ELOG,
					"failure to log expired event handler failure."
				);

			/* remove event if not persistent */
			if ((fly_event_unregister(e) == -1))
				return -1;
		}
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

__fly_static int __fly_expired_from_rbtree(fly_event_manager_t *manager, fly_rb_tree_t *tree, fly_rb_node_t *node, fly_time_t *__t)
{
	fly_event_t *__e;
	if (node == nil_node_ptr)
		return 0;

	while(node!=nil_node_ptr){
		struct __fly_event_for_rbtree *__er;

		__er = (struct __fly_event_for_rbtree *) node->key;
		fly_time_t *__et = __er->abs_timeout;

#ifdef DEBUG
		assert(__er != NULL);
		assert(__er->abs_timeout != NULL);
		assert(__er->ptr != NULL);
#endif
		if (__et->tv_sec > __t->tv_sec)
			node = node->c_left;
		else{
			__e = (fly_event_t *) node->data;
			__e->expired = true;

			__fly_expired_from_rbtree(manager, tree, node->c_left, __t);
			__fly_expired_from_rbtree(manager, tree, node->c_right, __t);
			return 0;
		}
//		switch(tree->cmp(node->key, __t, NULL)){
//		case FLY_RB_CMP_BIG:
//			node = node->c_left;
//			break;
//		case FLY_RB_CMP_SMALL:
//		case FLY_RB_CMP_EQUAL:
//			/* __n right partial tree is expired */
//			__e = (fly_event_t *) node->data;
//			__e->expired = true;
//			__fly_expired_from_rbtree(manager, tree, node->c_left, __t);
//			__fly_expired_from_rbtree(manager, tree, node->c_right, __t);
//			return 0;
//		default:
//			FLY_NOT_COME_HERE
//		}
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

__fly_static int __fly_event_handle_nomonitorable(fly_event_manager_t *manager)
{
	if (manager->unmonitorable.count == 0)
		return 0;

	struct fly_queue *__q;
	fly_event_t *__e;

	while(manager->unmonitorable.count>0){
		__q = manager->unmonitorable.next;
		__e = fly_queue_data(__q, struct fly_event, uqelem);
		fly_event_handle(__e);

		if (!fly_nodelete(__e)){
#ifdef DEBUG
			assert(fly_event_unregister(__e) != -1);
#else
			fly_event_unregister(__e);
#endif
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
	fly_event_t *le;

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

	le = fly_event_init(e->manager);
	if (fly_unlikely_null(le))
		return -1;
	FLY_EVENT_HANDLER(le, fly_log_event_handler);
	le->fd = fly_log_from_event(e)->notice->file;
	le->read_or_write = FLY_WRITE;
	le->flag = FLY_MODIFY;
	le->tflag = 0;
	le->eflag = 0;
	le->available = false;
	le->expired = false;
	le->event_data = (void *) lc;
	le->end_handler = NULL;
	le->end_event_data = NULL;
	fly_time_zero(le->timeout);
	fly_event_regular(le);

	if (fly_event_register(le) == -1)
		return -1;
	if (fly_event_unregister(e) == -1)
		return -1;

	return 0;
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
