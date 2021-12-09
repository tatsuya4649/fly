#include "context.h"
#include "response.h"
#include "connect.h"
#include "err.h"

__fly_static fly_sockinfo_t *__fly_listen_sock(fly_context_t *ctx, fly_pool_t *pool, struct fly_err *err);

fly_context_t *fly_context_init(struct fly_pool_manager *__pm, struct fly_err *err)
{
	fly_context_t *ctx;
	fly_pool_t *pool;

	ctx = fly_malloc(sizeof(fly_context_t));
	pool = fly_create_pool(__pm, FLY_CONTEXT_POOL_SIZE);
	pool->self_delete = true;

	memset(ctx, 0, sizeof(fly_context_t));
	ctx->pool = pool;
	ctx->event_pool = NULL;
	ctx->pool_manager = __pm;
	ctx->misc_pool = fly_create_pool(__pm, FLY_CONTEXT_POOL_SIZE);
	ctx->misc_pool->self_delete = true;
	ctx->listen_count = 0;
	ctx->listen_sock = __fly_listen_sock(ctx, pool, err);
	if (ctx->listen_sock == NULL)
		return NULL;
	ctx->max_response_content_length = fly_response_content_max_length();
	ctx->max_request_length = fly_max_request_length();
	ctx->request_timeout = fly_request_timeout();
	ctx->response_encode_threshold = fly_encode_threshold();
	ctx->log = fly_log_init(ctx, err);
	if (ctx->log == NULL)
		return NULL;
	ctx->route_reg = fly_route_reg_init(ctx);
	ctx->mount = NULL;
	ctx->log_stdout = fly_log_stdout();
	ctx->log_stderr = fly_log_stderr();

	ctx->emerge_ptr = fly_emerge_memory;
	fly_bllist_init(&ctx->rcbs);

	/* for SSL/TLS */
	ctx->ssl_ctx = NULL;
	ctx->daemon = false;
	/* ready for emergency error */
	fly_errsys_init(ctx);

	return ctx;
}

void fly_context_release(fly_context_t *ctx)
{
	if (ctx->listen_sock)
		close(ctx->listen_sock->fd);
	if (ctx->ssl_ctx)
		SSL_CTX_free(ctx->ssl_ctx);

	if (ctx->mount != NULL)
		fly_mount_release(ctx);

	fly_delete_pool(ctx->misc_pool);
	fly_delete_pool(ctx->pool);
	fly_free(ctx);
}

__fly_static fly_sockinfo_t *__fly_listen_sock(fly_context_t *ctx, fly_pool_t *pool, struct fly_err *err)
{
	fly_sockinfo_t *info;
	int port;

	info = fly_pballoc(pool, sizeof(fly_sockinfo_t));
	port = fly_server_port();
	if (fly_socket_init(ctx, port, info, fly_ssl(), err) == -1)
		return NULL;
	return info;
}

fly_rcbs_t *fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code)
{
	struct fly_bllist *__b;
	fly_rcbs_t *__r;

	fly_for_each_bllist(__b, &ctx->rcbs){
		__r = fly_bllist_data(__b, fly_rcbs_t, blelem);
		if (__r->status_code == status_code && __r->fd > 0)
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
