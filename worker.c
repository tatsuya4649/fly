#include "worker.h"

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx);
__fly_static int __fly_listen_socket_handler(struct fly_event *);
__fly_static fly_connect_t *__fly_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen);
__fly_static int __fly_listen_connected(fly_event_t *);


/*
 * this function is called after fork from master process.
 */
__noreturn void fly_worker_process(__unused fly_context_t *ctx, __unused void *data)
{
	fly_event_manager_t *manager;
	fly_event_t *event;

	if (!ctx)
		goto error_end;

	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		goto error_end;

	/* initial event */
	event = __fly_listen_socket_event(manager, ctx);
	if (event == NULL || fly_event_register(event) == -1)
		goto error_end;

	if (fly_event_handler(manager) == -1)
		goto error_end;

	fly_event_manager_release(manager);

	goto end;
error_end:
	exit(1);
end:
	exit(0);
}

__fly_static fly_event_t *__fly_listen_socket_event(fly_event_manager_t *manager, fly_context_t *ctx)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (e == NULL)
		return NULL;

	e->fd = ctx->listen_sock->fd;
	e->read_or_write = FLY_READ;
	/* TODO: accept */
	e->handler = __fly_listen_socket_handler;
	e->flag = FLY_PERSISTENT;
	e->tflag = FLY_INFINITY;
	e->eflag = 0;
	fly_time_null(e->timeout);
	e->event_data = ctx;

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
	ne->handler = __fly_listen_connected;
	ne->flag = FLY_NODELETE;
	ne->tflag = FLY_INFINITY;
	ne->eflag = 0;
	fly_time_null(ne->timeout);
	ne->event_data = __fly_connected(listen_sock, conn_sock, ne,(struct sockaddr *) &addr, addrlen);
	if (ne->event_data == NULL)
		return -1;

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
	fly_event_t *re;

	conn = (fly_connect_t *) e->event_data;
	/* ready for request */
	req = fly_request_init(conn);
	if (req == NULL)
		return -1;

	re = fly_event_init(e->manager);
	re->fd = e->fd;
	re->read_or_write = FLY_READ;
	re->event_data = req;
	/* event only modify (no add, no delete) */
	re->flag = FLY_MODIFY;
	re->tflag = 0;
	fly_sec(&re->timeout, FLY_REQUEST_TIMEOUT);
	re->eflag = 0;
	re->handler = fly_request_event_handler;
	re->event_state = (void *) EFLY_REQUEST_INIT;

	return fly_event_register(re);
}

