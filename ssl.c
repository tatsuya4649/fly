#define _GNU_SOURCE
#include <sys/socket.h>
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

int fly_listen_socket_ssl_handler(fly_event_t *e)
{
	fly_sock_t conn_sock;
	__unused fly_sockinfo_t *sockinfo;
	fly_sock_t listen_sock= e->fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
	int flag;
	SSL_CTX *ctx;
	SSL		*ssl;
	struct fly_ssl_accept *__ac;
	fly_event_t *ne;

	sockinfo = (fly_sockinfo_t *) e->event_data;
	/* SSL certificate/private key file setting */
	if (fly_unlikely_null(sockinfo->crt_path) || fly_unlikely_null(sockinfo->key_path))
		return -1;

	SSL_library_init();
	SSL_load_error_strings();

	/* create SSL context */
	ctx = SSL_CTX_new(SSLv23_server_method());
	SSL_CTX_use_certificate_file(ctx, sockinfo->crt_path, SSL_FILETYPE_PEM);
	SSL_CTX_use_PrivateKey_file(ctx, sockinfo->key_path, SSL_FILETYPE_PEM);
	if (SSL_CTX_check_private_key(ctx) != 1){
		/* TODO: Emerge log and end process */
		return -1;
	}

	SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_alpn_select_cb(ctx, __fly_ssl_alpn, NULL);

	addrlen = sizeof(struct sockaddr_storage);
	/* non blocking accept */
	flag = SOCK_NONBLOCK | SOCK_CLOEXEC;
	conn_sock = accept4(listen_sock, (struct sockaddr *) &addr, &addrlen, flag);
	if (conn_sock == -1)
		return -1;

	ssl = SSL_new(ctx);
	SSL_set_fd(ssl, conn_sock);

	__ac = fly_pballoc(e->manager->ctx->misc_pool, sizeof(struct fly_ssl_accept));
	if (fly_unlikely_null(__ac))
		return -1;
	__ac->manager = e->manager;
	__ac->pool = e->manager->ctx->misc_pool;
	__ac->ssl = ssl;
	__ac->ctx = ctx;
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
	if ((res=SSL_accept(__ac->ssl)) <= 0){
		/* TODO: log event */
		switch(SSL_get_error(__ac->ssl, res)){
		case SSL_ERROR_NONE:
			break;
		case SSL_ERROR_ZERO_RETURN:
			printf("SSL_ERROR_ZERO_RETURN\n");
			return 0;
		case SSL_ERROR_WANT_X509_LOOKUP:
			printf("SSL_ERROR_WANT_X509_LOOKUP\n");
			return 0;
		case SSL_ERROR_WANT_CLIENT_HELLO_CB:
			printf("SSL_ERROR_WANT_CLIENT_HELLO_CB\n");
			return 0;
		case SSL_ERROR_WANT_ACCEPT:
			printf("SSL_ERROR_ACCEPT\n");
			return 0;
		case SSL_ERROR_WANT_READ:
			printf("SSL_ERROR_READ\n");
			goto read_blocking;
		case SSL_ERROR_WANT_WRITE:
			printf("SSL_ERROR_WRITE\n");
			goto write_blocking;
		case SSL_ERROR_SYSCALL:
			printf("SSL_ERROR_SYSCALL\n");
			return -1;
		case SSL_ERROR_SSL:
			printf("SSL_ERROR_SSL\n");
			return -1;
		default:
			/* unknown error */
			return -1;
		}
	}

	e->fd = conn_sock;
	e->read_or_write = FLY_READ;
	FLY_EVENT_HANDLER(e, fly_listen_connected);
	e->flag = FLY_NODELETE;
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
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	e->expired = false;
	e->available = false;
	e->event_data = (struct fly_ssl_accept *) __ac;
	fly_event_socket(e);
	return fly_event_register(e);
}

__fly_static int __fly_ssl_accept_blocking_handler(fly_event_t *e)
{
	struct fly_ssl_accept *__ac;

	__ac = (struct fly_ssl_accept *) e->event_data;
	return __fly_ssl_accept_event_handler(e, __ac);
}
