#define _GNU_SOURCE
#include <sys/socket.h>
#include <openssl/err.h>
#include "ssl.h"
#include "server.h"
#include "request.h"
#include "event.h"
#include "connect.h"
#include "version.h"

__fly_static int __fly_ssl_alpn(SSL *ssl, const unsigned char **out, unsigned char *outlen, const unsigned char *in, unsigned int inlen, void *arg);
__fly_static int __fly_ssl_accept_blocking_handler(fly_event_t *e __fly_unused);

struct fly_ssl_accept{
	fly_pool_t				*pool;
	fly_connect_t			*connect;
	fly_event_manager_t 	*manager;
	SSL						*ssl;
	SSL_CTX					*ctx;
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
	if (SSL_CTX_check_private_key(ssl_ctx) != 1)
		FLY_SSL_EMERGENCY_ERROR(ctx);

#ifdef DEBUG
	printf("SSL CRT PATH: %s\n", sockinfo->crt_path);
	printf("SSL KEY PATH: %s\n", sockinfo->key_path);
#endif

	SSL_CTX_set_options(ssl_ctx, SSL_OP_NO_SSLv2);
    SSL_CTX_set_alpn_select_cb(ssl_ctx, __fly_ssl_alpn, NULL);
	return;
}

int fly_accept_end_timeout_handler(fly_event_t *e)
{
	fly_connect_t *conn;

	conn = (fly_connect_t *) e->expired_event_data;
	fly_ssl_connected_release(conn);

	return fly_listen_socket_end_handler(e);
}

int fly_accept_listen_socket_ssl_handler(fly_event_t *e, fly_connect_t *conn)
{
	struct fly_ssl_accept *__ac;
	SSL		*ssl;
	fly_context_t *context;

	context = e->manager->ctx;
	/* non blocking accept */
	ssl = SSL_new(context->ssl_ctx);
	SSL_set_fd(ssl, conn->c_sockfd);

	conn->ssl = ssl;
	conn->flag = FLY_SSL_CONNECT;

	FLY_EVENT_EXPIRED_END_HANDLER(e, fly_accept_end_timeout_handler, conn);
	__ac = fly_pballoc(e->manager->ctx->misc_pool, sizeof(struct fly_ssl_accept));
	if (fly_unlikely_null(__ac)){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ERR,
			"SSL/TLS connection setting error . (%s: %s)", __FILE__, __LINE__
		);
		fly_event_error_add(e, __err);
		return -1;
	}
	__ac->manager = e->manager;
	__ac->connect = conn;
	__ac->pool = e->manager->ctx->misc_pool;
	__ac->ssl = ssl;
	__ac->ctx = context->ssl_ctx;
	return __fly_ssl_accept_event_handler(e, __ac);
}

void fly_ssl_accept_free(SSL *ssl)
{
	SSL_free(ssl);
}

void fly_ssl_connected_release(fly_connect_t *conn)
{
	fly_ssl_accept_free(conn->ssl);
}

__fly_static int __fly_ssl_strcmp(char *d1, char *d2, size_t d1_len, size_t d2_len)
{
	if (d1_len != d2_len)
		return -1;

	while(*d1 == *d2){
		d1_len--;
		if (d1_len == 0)
			return 0;

		d1++;
		d2++;
	}
	return -1;
}

__fly_static int __fly_ssl_alpn_cmp(fly_http_version_t *__v, const unsigned char *in, unsigned inlen)
{
	char *ptr=(char *) in;
	while(true){
		char len=*ptr++;

		if (__fly_ssl_strcmp((char *) __v->alpn, ptr, strlen(__v->alpn), len) == 0)
			return 0;

		if (ptr+(int)len-1 >= (char *) in+inlen-1)
			return -1;
		else
			ptr+=(int)len;
	}
}

__fly_static int __fly_ssl_alpn(SSL *ssl __fly_unused, const unsigned char **out __fly_unused, unsigned char *outlen __fly_unused, const unsigned char *in __fly_unused, unsigned int inlen __fly_unused, void *arg __fly_unused)
{
	for (fly_http_version_t *__v=versions; __v->full; __v++){
		if (!__v->alpn)
			continue;
		if (__fly_ssl_alpn_cmp(__v, in, inlen) == 0){
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
	const unsigned char *data;
	unsigned int len;

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
			else if (errno == 0)
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

	SSL_get0_alpn_selected(__ac->ssl, &data, &len);
	__ac->connect->http_v = fly_match_version_from_alpn(data, len);
	if (fly_unlikely_null(__ac->connect->http_v)){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ALERT,
			"invalid alpn error in SSL/TLS negotiation."
		);
		fly_event_error_add(e, __err);
		fly_pbfree(__ac->pool, __ac);
		goto disconnect;
	}

	e->event_data = __ac->connect;
	/* release accept resource */
	fly_pbfree(__ac->pool, __ac);
	return fly_listen_connected(e);

read_blocking:
	e->read_or_write = FLY_READ;
	goto blocking;

write_blocking:
	e->read_or_write = FLY_WRITE;
	goto blocking;

blocking:
	e->fd = conn_sock;
	FLY_EVENT_HANDLER(e, __fly_ssl_accept_blocking_handler);
	e->flag = FLY_MODIFY;
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
	fly_connect_release(__ac->connect);
	fly_pbfree(__ac->pool, __ac);
	e->flag = FLY_CLOSE_EV;
	return 0;

disconnect:
	fly_ssl_accept_free(__ac->ssl);
	fly_pbfree(__ac->pool, __ac);
	e->flag = FLY_CLOSE_EV;
	return 0;
}

__fly_static int __fly_ssl_accept_blocking_handler(fly_event_t *e)
{
	struct fly_ssl_accept *__ac;

	__ac = (struct fly_ssl_accept *) e->event_data;
	return __fly_ssl_accept_event_handler(e, __ac);
}

__fly_noreturn void FLY_SSL_EMERGENCY_ERROR(fly_context_t *ctx)
{
	unsigned long err_code;

	while((err_code = ERR_peek_error())){
		struct fly_err *__err;

		char *err_content;
		err_content = ERR_error_string(err_code, NULL);

		__err = fly_err_init(ctx->pool, 0, FLY_ERR_ERR, "SSL error: %s", err_content);
		fly_error_error_noexit(__err);

		/* remove error entry from queue */
		ERR_get_error();
	}
	exit(1);
}

#define FLY_SSL_ERROR_LOG_TYPE				FLY_LOG_ERROR
#define FLY_SSL_ERROR_LOG_MAXLENGTH			(200)
int fly_ssl_error_log(fly_event_manager_t *manager)
{
	unsigned long err_code;
	fly_log_t *log;

	log = fly_log_from_manager(manager);
	while((err_code = ERR_peek_error())){
		/* register log event */
		fly_logcont_t *logcont;

		logcont = fly_logcont_init(log, FLY_SSL_ERROR_LOG_TYPE);
		fly_logcont_setting(logcont, FLY_SSL_ERROR_LOG_MAXLENGTH);

		ERR_error_string_n(err_code, logcont->content, logcont->contlen);

		/* event register */
		if (fly_log_event_register(manager, logcont) == -1)
			return -1;

		/* remove error entry from queue */
		ERR_get_error();
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
