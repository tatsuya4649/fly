#define _GNU_SOURCE
#include <sys/socket.h>
#include <openssl/err.h>
#include "ssl.h"
#include "server.h"
#include "request.h"
#include "event.h"
#include "connect.h"
#include "version.h"

__fly_static fly_connect_t *__fly_ssl_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen, SSL *ssl, SSL_CTX *ctx);
__fly_static int __fly_ssl_alpn(SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg);
__fly_static int __fly_ssl_accept_blocking_handler(fly_event_t *e __unused);

struct fly_ssl_accept{
	fly_pool_t *pool;
	fly_event_manager_t *manager;
	SSL *ssl;
	SSL_CTX *ctx;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int listen_sock;
};

__fly_static int __fly_ssl_accept_event_handler(fly_event_t *e, struct fly_ssl_accept *__ac);

void fly_listen_socket_ssl_setting(fly_context_t *ctx, fly_sockinfo_t *sockinfo)
{
	SSL_CTX *ssl_ctx;

	SSL_library_init();
	SSL_load_error_strings();

	/* create SSL context */
	ssl_ctx = SSL_CTX_new(SSLv23_server_method());
	ctx->ssl_ctx = ssl_ctx;
	SSL_CTX_use_certificate_file(ssl_ctx, sockinfo->crt_path, SSL_FILETYPE_PEM);
	SSL_CTX_use_PrivateKey_file(ssl_ctx, sockinfo->key_path, SSL_FILETYPE_PEM);
	if (SSL_CTX_check_private_key(ssl_ctx) != 1){
		/* TODO: Emerge log and end process */
		return;
	}

	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_alpn_select_cb(ssl_ctx, __fly_ssl_alpn, NULL);
	return;
}

int fly_listen_socket_ssl_handler(fly_event_t *e)
{
	fly_sock_t conn_sock;
	fly_sock_t listen_sock= e->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int flag;
	struct fly_ssl_accept *__ac;
	fly_event_t *ne;
	SSL		*ssl;
	fly_context_t *context;

	context = e->manager->ctx;
	addrlen = sizeof(struct sockaddr_storage);
	/* non blocking accept */
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
	if (conn_sock == -1)
		return -1;

	ssl = SSL_new(context->ssl_ctx);
	SSL_set_fd(ssl, conn_sock);

	__ac = fly_pballoc(e->manager->ctx->misc_pool, sizeof(struct fly_ssl_accept));
	if (fly_unlikely_null(__ac))
		return -1;
	__ac->manager = e->manager;
	__ac->pool = e->manager->ctx->misc_pool;
	__ac->ssl = ssl;
	__ac->ctx = context->ssl_ctx;
	__ac->listen_sock = listen_sock;
	memcpy(&__ac->addr, &addr, addrlen);
	__ac->addrlen = addrlen;
	ne = fly_event_init(__ac->manager);
	if (fly_unlikely_null(ne))
		return -1;
	/* start of request timeout */
	fly_sec(&ne->timeout, FLY_REQUEST_TIMEOUT);
	ne->tflag = 0;
	ne->eflag = 0;
	ne->expired = false;
	ne->available = false;
	return __fly_ssl_accept_event_handler(ne, __ac);
}

void fly_ssl_accept_free(SSL *ssl)
{
	SSL_free(ssl);
}

__fly_static fly_connect_t *__fly_ssl_connected(fly_sock_t fd, fly_sock_t cfd, fly_event_t *e, struct sockaddr *addr, socklen_t addrlen, SSL *ssl, SSL_CTX *ctx)
{
	fly_connect_t *conn;
	const unsigned char *data;
	unsigned int len;

	conn = fly_connect_init(fd, cfd, e, addr, addrlen);
	if (conn == NULL)
		return NULL;

	conn->ssl = ssl;
	conn->ssl_ctx = ctx;
	conn->flag = FLY_SSL_CONNECT;

	SSL_get0_alpn_selected(ssl, &data, &len);
	conn->http_v = fly_match_version_from_alpn(data, len);
	if (fly_unlikely_null(conn->http_v))
		return NULL;
	return conn;
}

void fly_ssl_connected_release(fly_connect_t *conn)
{
	fly_ssl_accept_free(conn->ssl);
}

__fly_static int __fly_ssl_strcmp(char *d1, char *d2, unsigned char max)
{
	while(max-- && *d1++ == *d2++)
		if (max == 0)
			return 0;
	return -1;
}

__fly_static int __fly_ssl_alpn_cmp(fly_http_version_t *__v, const unsigned char *in, unsigned inlen)
{
	char *ptr=(char *) in;
	while(true){
		char len=*ptr;

		if (__fly_ssl_strcmp((char *) __v->alpn, ptr, len) == 0)
			return 0;

		if (ptr+(int)len > (char *) in+inlen)
			return -1;
		else
			ptr+=(int)len;
	}
}

__fly_static int __fly_ssl_alpn(SSL *ssl __unused, const unsigned char **out __unused, unsigned char *outlen __unused, const unsigned char *in __unused, unsigned int inlen __unused, void *arg __unused)
{
	for (fly_http_version_t *__v=versions; __v->full; __v++){
		if (!__v->alpn)
			continue;
		if (__fly_ssl_alpn_cmp(__v, in, inlen)){
			*out = (unsigned char *) __v->alpn;
			*outlen = strlen(__v->alpn);
			return SSL_TLSEXT_ERR_OK;
		}
	}
	return SSL_TLSEXT_ERR_NOACK;
}


__fly_static int __fly_ssl_accept_event_handler(fly_event_t *e, struct fly_ssl_accept *__ac)
{
	int res, conn_sock;

	/* create new connected socket event. */
	conn_sock = SSL_get_fd(__ac->ssl);
	ERR_clear_error();
	if ((res=SSL_accept(__ac->ssl)) <= 0){
		switch(SSL_get_error(__ac->ssl, res)){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_WANT_READ:
#ifdef DEBUG
			printf("SSL_ERROR_READ\n");
#endif
			goto read_blocking;
		case SSL_ERROR_WANT_WRITE:
#ifdef DEBUG
			printf("SSL_ERROR_WRITE\n");
#endif
			goto write_blocking;
		case SSL_ERROR_SYSCALL:
#ifdef DEBUG
			printf("SSL_ERROR_SYSCALL\n");
#endif
			if (errno == EPIPE)
				goto disconnect;
			/* unexpected EOF from the peer */
			if (errno == 0)
				goto disconnect;
			goto connect_error;
		case SSL_ERROR_SSL:
#ifdef DEBUG
			printf("SSL_ERROR_SSL\n");
#endif
			goto connect_error;
		default:
#ifdef DEBUG
			printf("Unknown error\n");
#endif
			/* unknown error */
			goto connect_error;
		}
	}

	e->fd = conn_sock;
	e->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(e, fly_listen_connected);
	e->flag = FLY_NODELETE;
	if (fly_event_already_added(e))
		e->flag |= FLY_MODIFY;;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	e->expired = false;
	e->available = false;
	e->event_data = __fly_ssl_connected(__ac->listen_sock, conn_sock, e,(struct sockaddr *) &__ac->addr, __ac->addrlen, __ac->ssl, __ac->ctx);
	if (fly_unlikely_null(e->event_data))
		return -1;
	fly_event_socket(e);

	/* release accept resource */
	fly_pbfree(__ac->pool, __ac);
	return fly_event_register(e);

read_blocking:
	e->read_or_write = FLY_READ;
	goto blocking;

write_blocking:
	e->read_or_write = FLY_WRITE;
	goto blocking;

blocking:
	e->fd = conn_sock;
	FLY_EVENT_HANDLER(e, __fly_ssl_accept_blocking_handler);
	e->flag = FLY_NODELETE;
	if (fly_event_already_added(e))
		e->flag |= FLY_MODIFY;;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	e->expired = false;
	e->available = false;
	e->event_data = (struct fly_ssl_accept *) __ac;
	fly_event_socket(e);
	return fly_event_register(e);

connect_error:
	/* connect error log */
	fly_ssl_error_log(e->manager);
disconnect:
	fly_ssl_accept_free(__ac->ssl);
	fly_pbfree(__ac->pool, __ac);
	return fly_event_unregister(e);

}

__fly_static int __fly_ssl_accept_blocking_handler(fly_event_t *e)
{
	struct fly_ssl_accept *__ac;

	__ac = (struct fly_ssl_accept *) e->event_data;
	return __fly_ssl_accept_event_handler(e, __ac);
}

#define FLY_SSL_ERROR_LOG_TYPE				FLY_LOG_ERROR
#define FLY_SSL_ERROR_LOG_MAXLENGTH			(200)
int fly_ssl_error_log(fly_event_manager_t *manager)
{
	int err_code;
	fly_log_t *log;

	log = fly_log_from_manager(manager);
	while((err_code = ERR_peek_error())){
		/* register log event */
		fly_logcont_t *logcont;

		logcont = fly_logcont_init(log, FLY_SSL_ERROR_LOG_TYPE);
		if (fly_logcont_setting(logcont, FLY_SSL_ERROR_LOG_MAXLENGTH) == -1)
			return -1;

		ERR_error_string_n(err_code, logcont->content, logcont->contlen);

		/* event register */
		if (fly_log_event_register(manager, logcont) == -1)
			return -1;
	}

	return 0;
}

bool fly_ssl(void)
{
	return fly_config_value_bool(FLY_SSL);
}

char *fly_ssl_crt_path(void)
{
	return fly_config_value_str(FLY_SSL_CRT_PATH);
}

char *fly_ssl_key_path(void)
{
	return fly_config_value_str(FLY_SSL_KEY_PATH);
}
