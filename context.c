#include "context.h"
#include "response.h"
#include <sys/sendfile.h>

__fly_static fly_sockinfo_t *__fly_listen_sock(fly_context_t *ctx, fly_pool_t *pool);
int fly_errsys_init(fly_context_t *ctx);
__fly_static int __fly_send_dcbs_blocking(fly_event_t *e, fly_response_t *res, int read_or_write);
__fly_static int __fly_send_dcbs_blocking_handler(fly_event_t *e);

fly_context_t *fly_context_init(struct fly_pool_manager *__pm)
{
	fly_context_t *ctx;
	fly_pool_t *pool;

	pool = fly_create_pool(__pm, FLY_CONTEXT_POOL_SIZE);
	if (!pool)
		return NULL;

	ctx = fly_pballoc(pool, sizeof(fly_context_t));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(fly_context_t));
	ctx->pool = pool;
	ctx->pool_manager = __pm;
	ctx->misc_pool = fly_create_pool(__pm, FLY_CONTEXT_POOL_SIZE);
	if (!ctx->misc_pool)
		return NULL;
	FLY_DUMMY_SOCK_INIT(ctx);
	ctx->listen_sock = __fly_listen_sock(ctx, pool);
	if (ctx->listen_sock == NULL)
		return NULL;
	ctx->route_reg = fly_route_reg_init(ctx);
	if (ctx->route_reg == NULL)
		return NULL;
	ctx->log = fly_log_init(ctx);
	if (ctx->log == NULL)
		return NULL;
	fly_bllist_init(&ctx->rcbs);

	/* ready for emergency error */
	if (fly_errsys_init(ctx) == -1)
		return NULL;

	return ctx;
}

void fly_context_release(fly_context_t *ctx)
{
	fly_delete_pool(ctx->pool);
}

/* configuration file add. */
__fly_static fly_sockinfo_t *__fly_listen_sock(fly_context_t *ctx, fly_pool_t *pool)
{
	fly_sockinfo_t *info;
	int port;

	info = fly_pballoc(pool, sizeof(fly_sockinfo_t));
	if (!info)
		return NULL;

	port = fly_server_port();
	if (fly_socket_init(ctx, port, info, fly_ssl()) == -1)
		return NULL;
	return info;
}

fly_rcbs_t *fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code)
{
	struct fly_bllist *__b;
	fly_rcbs_t *__r;

	fly_for_each_bllist(__b, &ctx->rcbs){
		__r = fly_bllist_data(__b, fly_rcbs_t, blelem);
		if (__r->status_code == status_code || __r->fd > 0)
			return __r;
	}
	return NULL;
}

fly_rcbs_t *fly_default_content_by_stcode_from_event(fly_event_t *e, enum status_code_type status_code)
{
	return fly_default_content_by_stcode(e->manager->ctx, status_code);
}

int is_fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code)
{
	return fly_default_content_by_stcode(ctx, status_code) ? 1 : 0;
}

__fly_static int __fly_send_dcbs_blocking(fly_event_t *e, fly_response_t *res, int read_or_write)
{
	e->event_data = (void *) res;
	e->read_or_write = read_or_write;
	e->eflag = 0;
	e->tflag = FLY_INHERIT;
	e->flag = FLY_NODELETE;
	e->available = false;
	FLY_EVENT_HANDLER(e, __fly_send_dcbs_blocking_handler);
	return fly_event_register(e);
}

__fly_static int __fly_send_dcbs_blocking_handler(fly_event_t *e)
{
	fly_response_t *response;

	response = (fly_response_t *) e->event_data;
	return fly_send_default_content_by_stcode(e, response->status_code);
}

int fly_send_default_content_by_stcode(fly_event_t *e, enum status_code_type status_code)
{
	fly_context_t *ctx;
	struct fly_bllist *__b;
	fly_rcbs_t *__r;

	ctx = e->manager->ctx;

	fly_for_each_bllist(__b, &ctx->rcbs){
		__r = fly_bllist_data(__b, fly_rcbs_t, blelem);
		if (__r->status_code == status_code || __r->fd > 0)
			return fly_send_default_content(e, __r);
	}

	return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_NOTFOUND;
}

int fly_send_default_content(fly_event_t *e, fly_rcbs_t *__r)
{
	fly_response_t *res;
	struct stat sb;
	size_t total, count;
	ssize_t numsend;
	off_t *offset;

	res = (fly_response_t *) e->event_data;
	if (fstat(__r->fd, &sb) == -1)
		return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR;
	if (sb.st_size == 0)
		return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_SUCCESS;

	offset = &res->offset;

	if (*offset)
		total = (size_t) *offset;
	else
		total = 0;

	count = sb.st_size - total;
	while(total < count){
		if (FLY_CONNECT_ON_SSL(res->request->connect)){
#define FLY_SEND_BUF_LENGTH			(4096)
			//numsend = SSL_sendfile(ssl, __r->fd, offset, count-total, 0);
			SSL *ssl=res->request->connect->ssl;
			char send_buf[FLY_SEND_BUF_LENGTH];
			ssize_t numread;

			if (lseek(__r->fd, *offset+(off_t) total, SEEK_SET) == -1)
				return FLY_RESPONSE_ERROR;
			numread = read(__r->fd, send_buf, count-total<FLY_SEND_BUF_LENGTH ? FLY_SEND_BUF_LENGTH : count-total);
			if (numread == -1)
				return FLY_RESPONSE_ERROR;
			numsend = SSL_write(ssl, send_buf, numread);
			switch(SSL_get_error(ssl, numsend)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_WANT_READ:
				if (__fly_send_dcbs_blocking(e, res, FLY_READ) == -1)
					return FLY_RESPONSE_ERROR;
				return FLY_RESPONSE_BLOCKING;
			case SSL_ERROR_WANT_WRITE:
				if (__fly_send_dcbs_blocking(e, res, FLY_WRITE) == -1)
					return FLY_RESPONSE_ERROR;
				return FLY_RESPONSE_BLOCKING;
			case SSL_ERROR_SYSCALL:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_SSL:
				return FLY_RESPONSE_ERROR;
			default:
				/* unknown error */
				return FLY_RESPONSE_ERROR;
			}
			*offset += numsend;
		}else{
			numsend = sendfile(e->fd, __r->fd, offset, count-total);
			if (FLY_BLOCKING(numsend)){
				/* event register */
				if (__fly_send_dcbs_blocking(e, res, FLY_WRITE) == -1)
					return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR;
				return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_BLOCKING;
			}else if (numsend == -1)
				return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR;
		}

		total += numsend;
	}

	*offset = 0;
	res->fase = FLY_RESPONSE_RELEASE;
	return FLY_SEND_DEFAULT_CONTENT_BY_STCODE_SUCCESS;
}
