#define _GNU_SOURCE
#include "connect.h"
#include <stdio.h>
#include <sys/socket.h>
#include "server.h"
#include "event.h"
#include "request.h"
#include "v2.h"
#include "ssl.h"
#include "response.h"

__fly_static int __fly_info_of_connect(fly_connect_t *conn);

fly_connect_t *fly_connect_init(int sockfd, int c_sockfd, fly_event_t *event, struct sockaddr *addr, socklen_t addrlen)
{
	fly_pool_t *pool;
	fly_connect_t *conn;

	pool = fly_create_pool(FLY_POOL_MANAGER_FROM_EVENT(event), FLY_CONNECTION_POOL_SIZE);
	conn = fly_pballoc(pool, sizeof(fly_connect_t));
	if (conn == NULL)
		return NULL;

	conn->event = event;
	conn->sockfd = sockfd;
	conn->c_sockfd = c_sockfd;
	conn->pool = pool;
	memcpy(&conn->peer_addr, addr, addrlen);
	conn->addrlen = addrlen;
	conn->flag = 0;

	/* for HTTP2 */
	conn->ssl_ctx = NULL;
	conn->ssl = NULL;
	conn->http_v = fly_default_http_version();
	conn->v2_state = NULL;
	conn->peer_closed = false;
#define FLY_CONNECT_BUFFER_INIT_LEN			1
#define FLY_CONNECT_BUFFER_CHAIN_MAX		100
#define FLY_CONNECT_BUFFER_PER_LEN			10
	conn->buffer = fly_buffer_init(pool, FLY_CONNECT_BUFFER_INIT_LEN, FLY_CONNECT_BUFFER_CHAIN_MAX, FLY_CONNECT_BUFFER_PER_LEN);
	if (fly_unlikely_null(conn->buffer))
		return NULL;

	if (__fly_info_of_connect(conn) == -1)
		return NULL;

	return conn;
}

int fly_connect_release(fly_connect_t *conn)
{
	if (conn == NULL)
		return -1;

	/* SSL/TLS release */
	if (conn->flag & FLY_SSL_CONNECT)
		fly_ssl_connected_release(conn);

	if (fly_socket_close(conn->c_sockfd, FLY_SOCK_CLOSE) == -1)
		return -1;

	fly_delete_pool(conn->pool);
	return 0;
}

__fly_static int __fly_info_of_connect(fly_connect_t *conn)
{
	int gname_err;
	gname_err=getnameinfo(
		(struct sockaddr *) &conn->peer_addr,
		conn->addrlen,
		conn->hostname,
		NI_MAXHOST,
		conn->servname,
		NI_MAXSERV,
		NI_NUMERICHOST|NI_NUMERICSERV
	);
	if (gname_err != 0){
		return -1;
	}
	return 0;
}

/*
 *  ready for receiving request, and publish an received event.
 */
int fly_listen_connected(fly_event_t *e)
{
	fly_connect_t *conn;
	fly_request_t *req;

	conn = (fly_connect_t *) e->event_data;
	e->fd = e->fd;
	e->read_or_write = FLY_READ;
	/* event only modify (no add, no delete) */
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;

	switch(FLY_CONNECT_HTTP_VERSION(conn)){
	case V1_1:
		req = fly_request_init(conn);
		if (req == NULL)
			return -1;
		e->event_data = (void *) req;
		/* ready for request */
		FLY_EVENT_HANDLER(e, fly_request_event_handler);
		break;
	case V2:
		e->event_data = (void *) conn;
		FLY_EVENT_HANDLER(e, fly_hv2_init_handler);
		break;
	default:
		FLY_NOT_COME_HERE
	}
	e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
	e->event_fase = (void *) EFLY_REQUEST_FASE_INIT;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return fly_event_register(e);
}

static fly_connect_t *fly_http_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen)
{
	fly_connect_t *conn;

	conn = fly_connect_init(fd, cfd, e, addr, addrlen);
	if (conn == NULL)
		return NULL;

	return conn;
}


int fly_accept_listen_socket_handler(struct fly_event *event)
{
	fly_connect_t *conn;
	fly_sock_t conn_sock;
	fly_sock_t listen_sock = event->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int flag;
	fly_event_t *ne;
	fly_sockinfo_t *sockinfo;

	sockinfo = (fly_sockinfo_t *) event->event_data;
	addrlen = sizeof(struct sockaddr_storage);
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
	if (conn_sock == -1){
		if (FLY_BLOCKING(conn_sock))
			goto read_blocking;
		else
			return -1;
	}

	/* check TLS or HTTP */
#define FLY_TLS_HTTP_CHECK_BUFLEN			1
	char buf[FLY_TLS_HTTP_CHECK_BUFLEN];
	ssize_t n;

	n = recv(conn_sock, buf, FLY_TLS_HTTP_CHECK_BUFLEN, MSG_DONTWAIT|MSG_PEEK);
	if (n != FLY_TLS_HTTP_CHECK_BUFLEN){
		if (FLY_BLOCKING(conn_sock))
			goto read_blocking;
		else if (n == 0)
			goto disconnect;
		else
			return -1;
	}

	if (fly_tls_handshake_magic(buf)){
		/* HTTP request over TLS */
		return fly_accept_listen_socket_ssl_handler(event);
	}else{
		/* HTTP request */
		if (sockinfo->flag & FLY_SOCKINFO_SSL)
			goto response_400;

		/* create new connected socket event. */
		ne = fly_event_init(event->manager);
		if (ne == NULL)
			return -1;
		ne->fd = conn_sock;
		ne->read_or_write = FLY_READ;
		FLY_EVENT_HANDLER(ne, fly_listen_connected);
		ne->flag = FLY_NODELETE;
		fly_sec(&ne->timeout, FLY_REQUEST_TIMEOUT);
		ne->tflag = 0;
		ne->eflag = 0;
		ne->expired = false;
		ne->available = false;
		conn = fly_http_connected(listen_sock, conn_sock, ne,(struct sockaddr *) &addr, addrlen);
		ne->event_data = conn;
		if (ne->event_data == NULL)
			return -1;
		fly_event_socket(ne);

		return fly_event_register(ne);
	}
read_blocking:
	event->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(event, fly_accept_listen_socket_handler);
	return fly_event_register(event);

disconnect:
	return 0;

response_400:
	conn = fly_http_connected(listen_sock, conn_sock, event,(struct sockaddr *) &addr, addrlen);
	return fly_400_event_norequest(event, conn);
}
