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
	fly_context_t *ctx;

	ctx = fly_context_from_event(event);
	pool = fly_create_pool(FLY_POOL_MANAGER_FROM_EVENT(event), FLY_CONNECTION_POOL_SIZE);
	conn = fly_pballoc(pool, sizeof(fly_connect_t));
	if (fly_unlikely_null(conn))
		return NULL;
	conn->event = event;
	conn->sockfd = sockfd;
	conn->c_sockfd = c_sockfd;
	conn->pool = pool;
	memcpy(&conn->peer_addr, addr, addrlen);
	conn->addrlen = addrlen;
	conn->flag = 0;
	memset(conn->hostname, '\0', NI_MAXHOST);
	memset(conn->servname, '\0', NI_MAXSERV);

	/* for HTTP2 */
	conn->ssl_ctx = NULL;
	conn->ssl = NULL;
	conn->http_v = fly_default_http_version();
	conn->v2_state = NULL;
	conn->peer_closed = false;

	/* setting buffer length */
	conn->buffer_init_len = fly_connect_buffer_init_len();
	conn->buffer_per_len = fly_connect_buffer_per_len();

#define FLY_CONNECT_BUFFER_CHAIN_MAX(__ctx, per_len)			\
			((size_t) (((int) ((__ctx)->max_request_length/(per_len)))+1))
	conn->buffer = fly_buffer_init(pool, conn->buffer_init_len, FLY_CONNECT_BUFFER_CHAIN_MAX(ctx, conn->buffer_per_len), conn->buffer_per_len);
	if (fly_unlikely_null(conn->buffer))
		return NULL;

#ifdef DEBUG
	assert(conn->buffer_per_len > 0);
	assert(conn->buffer_init_len > 0);
	assert((conn->buffer_per_len*FLY_CONNECT_BUFFER_CHAIN_MAX(ctx, conn->buffer_per_len)) > ctx->max_request_length);
#endif
	if (__fly_info_of_connect(conn) == -1)
		return NULL;

	return conn;
}

void fly_connect_buffer_refresh(fly_connect_t *conn)
{
	fly_buffer_release(conn->buffer);

	conn->buffer = fly_buffer_init(conn->pool, conn->buffer_init_len, FLY_CONNECT_BUFFER_CHAIN_MAX(conn->event->manager->ctx, conn->buffer_per_len), conn->buffer_per_len);
	if (fly_unlikely_null(conn->buffer))
		FLY_EXIT_ERROR(
			"connection buffer refresh error. (%s: %d)",
			__FILE__, __LINE__
		);

	return;
}

int fly_connect_release(fly_connect_t *conn)
{
#ifdef DEBUG
	assert(conn);
#endif

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
#ifdef DEBUG
	printf("WORKER: Listen socket is connected\n");
#endif
	fly_connect_t *conn;
	fly_request_t *req;

	//conn = (fly_connect_t *) e->event_data;
	conn = (fly_connect_t *) fly_event_data_get(e, __p);
	e->read_or_write = FLY_READ;
	/* event only modify (no add, no delete) */
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	//e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
	//e->event_fase = (void *) EFLY_REQUEST_FASE_INIT;
	fly_event_state_set(e, __e, EFLY_REQUEST_STATE_INIT);
	fly_event_fase_set(e, __e, EFLY_REQUEST_FASE_INIT);
	fly_event_socket(e);

	switch(FLY_CONNECT_HTTP_VERSION(conn)){
	case V1_1:
#ifdef DEBUG
	printf("WORKER: Start HTTP1.1 communication\n");
#endif
		req = fly_request_init(conn);
		if (fly_unlikely_null(req)){
			struct fly_err *__err;
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"request init error."
			);
			fly_event_error_add(e, __err);
			return -1;
		}
		fly_event_data_set(e, __p, (void *) req);
		//e->event_data = (void *) req;

		e->fail_close = fly_request_fail_close_handler;
		FLY_EVENT_EXPIRED_END_HANDLER(e, fly_request_timeout_handler, req);
		return fly_request_event_handler(e);
	case V2:
#ifdef DEBUG
	printf("WORKER: Start HTTP2 communication\n");
#endif
		//e->event_data = (void *) conn;
		fly_event_data_set(e, __p, conn);
		FLY_EVENT_END_HANDLER(e, fly_hv2_end_handle, conn);
		FLY_EVENT_EXPIRED_HANDLER(e, fly_hv2_timeout_handle, conn);
		return fly_hv2_init_handler(e);
	default:
		FLY_NOT_COME_HERE
	}
}

int fly_fail_recognize_protocol(fly_event_t *e, int fd __fly_unused)
{
	fly_connect_t *con;

	con = (fly_connect_t *) fly_expired_event_data_get(e, __p);
	//con = (fly_connect_t *) e->expired_event_data;
	return fly_connect_release(con);
}

static int fly_recognize_protocol_of_connected(fly_event_t *e);
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
	fly_connect_t		*conn;
	fly_context_t		*ctx;
	fly_sock_t			conn_sock;
	fly_sock_t			listen_sock = event->fd;
	struct sockaddr_storage addr;
	socklen_t			addrlen;
	fly_event_t			*ne;

#ifdef DEBUG
	printf("WORKER: ACCEPT LISTEN EVENT\n");
	assert(event->err_count == 0);
#endif
	ctx = fly_context_from_event(event);
	addrlen = sizeof(struct sockaddr_storage);
	memset(&addr, '\0', sizeof(addr));
#ifdef HAVE_ACCEPT4
	int	flag;
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
#else
	int __sockval;
	conn_sock = accept(listen_sock, (struct sockaddr *) &addr, &addrlen);
	/* set non blocking */
	__sockval = fcntl(conn_sock, F_GETFL, 0);
	if (__sockval == -1)
		return -1;

	if (fcntl(conn_sock, F_SETFL, __sockval|O_NONBLOCK) == -1)
		return -1;
#endif
	if (conn_sock == -1){
		if (FLY_BLOCKING(conn_sock))
			goto read_blocking;
		else if (conn_sock == EMFILE || conn_sock == ENFILE)
			goto read_blocking;
		else{
			struct fly_err	*error;
			error = fly_event_err_init(event, errno, FLY_ERR_ERR, "an error occurred while accepting a listening socket(%s: %d)", __FILE__, __LINE__);
			fly_event_error_add(event, error);
			return FLY_EVENT_HANDLE_FAILURE;
		}
	}

	/* create new connected socket event. */
	ne = fly_event_init(event->manager);
	if (ne == NULL){
		struct fly_err	*error;
		error = fly_event_err_init(event, errno, FLY_ERR_EMERG, "an error occurred in event init (%s: %d)", __FILE__, __LINE__);
		fly_event_error_add(event, error);
		return FLY_EVENT_HANDLE_FAILURE;
	}
	ne->fd = conn_sock;
	ne->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(ne, fly_recognize_protocol_of_connected);
	ne->flag = 0;
	fly_sec(&ne->timeout, ctx->request_timeout);
	ne->tflag = 0;
	ne->eflag = 0;
	ne->expired = false;
	ne->available = false;
	conn = fly_http_connected(listen_sock, conn_sock, ne,(struct sockaddr *) &addr, addrlen);
	/* for end of connection */
	FLY_EVENT_EXPIRED_HANDLER(ne, fly_listen_socket_end_handler, conn);
	FLY_EVENT_END_HANDLER(ne, fly_listen_socket_end_handler, conn);
	fly_event_data_set(ne, __p, conn);
	fly_expired_event_data_set(ne, __p, conn);
	ne->fail_close = fly_fail_recognize_protocol;
	fly_event_socket(ne);

	return fly_event_register(ne);

read_blocking:
	return 0;
}

static int fly_recognize_protocol_of_connected(fly_event_t *e)
{
#ifdef DEBUG
	printf("WORKER: RECOGNIZE PROTOCOL EVENT\n");
#endif
	fly_connect_t *conn;
	fly_sock_t conn_sock;
	fly_sockinfo_t *sockinfo;

	//conn = (fly_connect_t *) e->event_data;
	conn = (fly_connect_t *) fly_event_data_get(e, __p);
	conn_sock = conn->c_sockfd;
	sockinfo = e->manager->ctx->listen_sock;

	/* check TLS or HTTP */
#define FLY_TLS_HTTP_CHECK_BUFLEN			1
	char buf[FLY_TLS_HTTP_CHECK_BUFLEN];
	ssize_t n;

	n = recv(conn_sock, buf, FLY_TLS_HTTP_CHECK_BUFLEN, MSG_PEEK);
	if (n != FLY_TLS_HTTP_CHECK_BUFLEN){
		if (FLY_BLOCKING(n))
			goto read_blocking;
		else if (n == 0)
			goto disconnect;
		else{
			struct fly_err *__err;
			__err = fly_event_err_init(e, errno, FLY_ERR_ERR, "recv error in recognizeing protocol of connection(%s: %d)", __FILE__, __LINE__);
			fly_event_error_add(e, __err);
			return -1;
		}
	}

	if (fly_tls_handshake_magic(buf)){
		if (!(sockinfo->flag & FLY_SOCKINFO_SSL)){
#ifdef DEBUG
			printf("Illegal request(HTTP server but HTTPS request). disconnect.\n");
#endif
			goto disconnect;
		}

#ifdef DEBUG
		printf("WORKER: SSL Received!\n");
#endif
		/* HTTP request over TLS */
		return fly_accept_listen_socket_ssl_handler(e, conn);
	}else{
		/* HTTP request */
		if (sockinfo->flag & FLY_SOCKINFO_SSL){
#ifdef DEBUG
			printf("Illegal request(HTTPS server but HTTP request). response 400.\n");
#endif
			goto response_400;
		}

#ifdef DEBUG
		printf("WORKER: HTTP Received!\n");
#endif
		return fly_listen_connected(e);
	}
read_blocking:
	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_recognize_protocol_of_connected);
	return fly_event_register(e);

disconnect:
	if (fly_connect_release(conn) == -1){
		struct fly_err *__err;
		__err = fly_event_err_init(e, errno, FLY_ERR_ERR, "release resources of connection error in recognizeing protocol of connection(%s: %d)", __FILE__, __LINE__);
		fly_event_error_add(e, __err);
		return -1;
	}

	e->flag = FLY_CLOSE_EV;
	return 0;

response_400:
	return fly_400_event_norequest(e, conn);
}

int fly_listen_socket_end_handler(fly_event_t *__e)
{
	fly_connect_t *conn;

	//conn = (fly_connect_t *) __e->end_event_data;
	conn = (fly_connect_t *) fly_end_event_data_get(__e, __p);

	__e->flag = FLY_CLOSE_EV;
	return fly_connect_release(conn);
}

size_t fly_max_request_length(void)
{
	return (size_t) fly_config_value_int(FLY_MAX_REQUEST_LENGTH);
}

size_t fly_connect_buffer_init_len(void)
{
	return (size_t) fly_config_value_int(FLY_CONNECT_BUFFER_INIT_LEN);
}

size_t fly_connect_buffer_per_len(void)
{
	return (size_t) fly_config_value_int(FLY_CONNECT_BUFFER_PER_LEN);
}
