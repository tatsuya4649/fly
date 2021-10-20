#include "connect.h"
#include <stdio.h>
#include <sys/socket.h>
#include "server.h"
#include "event.h"
#include "request.h"
#include "v2.h"
#include "ssl.h"

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
	conn->http_v = NULL;
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
		NI_NUMERICHOST
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
	if (conn->flag & FLY_SSL_CONNECT){
		e->event_data = (void *) conn;
		FLY_EVENT_HANDLER(e, fly_hv2_init_handler);
	}else{
		req = fly_request_init(conn);
		if (req == NULL)
			return -1;
		e->event_data = (void *) req;
		/* ready for request */
		FLY_EVENT_HANDLER(e, fly_request_event_handler);
	}
	e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
	e->event_fase = (void *) EFLY_REQUEST_FASE_INIT;
	e->expired = false;
	e->available = false;
	fly_event_socket(e);

	return fly_event_register(e);
}
