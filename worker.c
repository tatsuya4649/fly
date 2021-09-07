#include "worker.h"

__noreturn void fly_worker_process(fly_context_t *ctx, void *data);
__fly_static fly_event_t *__fly_listen_socked_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo);
__fly_static int __fly_listen_socket_handler(struct fly_event *);

/*
 * this function is called after fork from master process.
 */
__noreturn void fly_worker_process(__unused fly_context_t *ctx, __unused void *data)
{
	fly_event_manager_t *manager;
	fly_event_t *event;

	if (!ctx)
		goto error_end;

	manager = fly_event_manager_init();
	if (manager == NULL)
		goto error_end;

	event = __fly_listen_socked_event(manager, ctx->listen_sock);
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

__fly_static fly_event_t *__fly_listen_socked_event(fly_event_manager_t *manager, fly_sockinfo_t *sockinfo)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (e == NULL)
		return NULL;

	e->fd = sockinfo->fd;
	e->read_or_write = FLY_READ;
	/* TODO: accept */
	e->handler = __fly_listen_socket_handler;
	e->flag = FLY_PERSISTENT;

	return e;
}

__fly_static int __connected(fly_event_t *)
{
	printf("Hello World\n");
	return 0;
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
	/* TODO: connected socket handler */
	ne->handler = __connected;
	ne->flag = 0;

	if (fly_event_register(ne) == -1)
		return -1;

	return 0;
}

