#include "worker.h"
#include "fsignal.h"

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_listen_socket_handler(struct fly_event *);
__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen);
__fly_static int __fly_listen_connected(fly_event_t *);
__fly_static int __fly_worker_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx);
__fly_static int __fly_worker_signal_handler(fly_event_t *e);


#define FLY_WORKER_SIG_COUNT				(sizeof(fly_worker_signals)/sizeof(fly_signal_t))
/*
 *  worker process signal info.
 */
static fly_signal_t fly_worker_signals[] = {
	{SIGINT, NULL},
	{SIGTERM, NULL},
};

__fly_static int __fly_wsignal_handle(fly_context_t *ctx, struct signalfd_siginfo *info)
{
	for (int i=0;(int) FLY_WORKER_SIG_COUNT; i++){
		fly_signal_t *__s = &fly_worker_signals[i];
		if (__s->number == (fly_signum_t) info->ssi_signo){
			if (__s->handler)
				__s->handler(ctx, info);
			else
				fly_signal_default_handler(info);
		}
	}
	return 0;
}

__fly_static int __fly_worker_signal_handler(__unused fly_event_t *e)
{
	struct signalfd_siginfo info;
	ssize_t res;

	while(1){
		res = read(e->fd, (void *) &info,sizeof(struct signalfd_siginfo));
		if (res == -1){
			if (errno == EAGAIN || errno == EWOULDBLOCK)
				break;
			else
				return -1;
		}
		if (__fly_wsignal_handle(e->manager->ctx, &info) == -1)
			return -1;
	}

	return 0;
}

__fly_static int __fly_worker_signal_event(fly_event_manager_t *manager, __unused fly_context_t *ctx)
{
	sigset_t sset;
	int sigfd;
	fly_event_t *e;

	if (!manager ||  !manager->pool || !ctx)
		return -1;

	if (fly_refresh_signal() == -1)
		return -1;
	if (sigemptyset(&sset) == -1)
		return -1;

	for (int i=0; i<(int) FLY_WORKER_SIG_COUNT; i++){
		if (sigaddset(&sset, fly_worker_signals[i].number) == -1)
			return -1;
	}

	sigfd = fly_signal_register(&sset);
	if (sigfd == -1)
		return -1;

	e = fly_event_init(manager);
	if (e == NULL)
		return -1;

	e->fd = sigfd;
	e->read_or_write = FLY_READ;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	e->flag = FLY_PERSISTENT;
	e->event_fase = NULL;
	e->event_state = NULL;
	e->expired = false;
	e->available = false;
	e->handler = __fly_worker_signal_handler;

	fly_time_null(e->timeout);
	fly_event_file_type(e, SIGNAL);
	return fly_event_register(e);
}

/*
 * this function is called after fork from master process.
 * @params:
 *		ctx:  passed from master process. include fly context info.
 *		data: custom data.
 */
__direct_log __noreturn void fly_worker_process(fly_context_t *ctx, __unused void *data)
{
	fly_event_manager_t *manager;
	fly_event_t *event;

	if (!ctx)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"invalid context(null context)."
		);

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize event manager error."
		);

	/* initial event */
	/* signal setting */
	if (__fly_worker_signal_event(manager, ctx) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_READY,
			"initialize worker signal error."
		);

	event = __fly_listen_socket_event(manager, ctx);
	if (event == NULL || fly_event_register(event) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"fail to register listen socket event."
		);

	/* log event start here */
	if (fly_event_handler(manager) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_PROCS,
			"event handle error."
		);

	/* will not come here. */
	FLY_NOT_COME_HERE
}

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (e == NULL)
		return NULL;

	e->fd = ctx->listen_sock->fd;
	e->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(e, __fly_listen_socket_handler);
	e->flag = FLY_PERSISTENT;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	fly_time_null(e->timeout);
	e->event_data = ctx;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return e;
}

__fly_static int __fly_listen_socket_handler(__unused struct fly_event *event)
{
	fly_sock_t conn_sock;
	fly_sock_t listen_sock = event->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int flag;
	fly_event_t *ne;

	addrlen = sizeof(struct sockaddr_storage);
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
	if (conn_sock == -1)
		return -1;

	/* create new connected socket event. */
	ne = fly_event_init(event->manager);
	if (ne == NULL)
		return -1;
	ne->fd = conn_sock;
	ne->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(ne, __fly_listen_connected);
	ne->flag = FLY_NODELETE;
	ne->tflag = FLY_INFINITY;
	ne->eflag = 0;
	fly_time_null(ne->timeout);
	ne->expired = false;
	ne->available = false;
	ne->event_data = __fly_connected(listen_sock, conn_sock, ne,(struct sockaddr *) &addr, addrlen);
	if (ne->event_data == NULL)
		return -1;
	fly_event_socket(ne);

	if (fly_event_register(ne) == -1)
		return -1;

	return 0;
}

__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen)
{
	fly_connect_t *conn;

	conn = fly_connect_init(fd, cfd, e, addr, addrlen);
	if (conn == NULL)
		return NULL;

	return conn;
}

/*
 *  ready for receiving request, and publish an received event.
 */
__fly_static int __fly_listen_connected(fly_event_t *e)
{
	fly_connect_t *conn;
	fly_request_t *req;

	conn = (fly_connect_t *) e->event_data;
	/* ready for request */
	req = fly_request_init(conn);
	if (req == NULL)
		return -1;

	e->fd = e->fd;
	e->read_or_write = FLY_READ;
	e->event_data = (void *) req;
	/* event only modify (no add, no delete) */
	e->flag = FLY_MODIFY;
	e->tflag = 0;
	fly_sec(&e->timeout, FLY_REQUEST_TIMEOUT);
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_request_event_handler);
	e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
	e->event_fase = (void *) EFLY_REQUEST_FASE_INIT;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return fly_event_register(e);
}

