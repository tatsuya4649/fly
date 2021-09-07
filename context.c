
#include "context.h"

__fly_static fly_sockinfo_t *__fly_listen_sock(fly_pool_t *pool);

fly_context_t *fly_context_init(void)
{
	fly_context_t *ctx;
	fly_pool_t *pool;

	pool = fly_create_pool(FLY_CONTEXT_POOL_SIZE);
	if (!pool)
		return NULL;

	ctx = fly_pballoc(pool, sizeof(fly_context_t));
	if (!ctx)
		return NULL;

	memset(ctx, 0, sizeof(fly_context_t));
	ctx->pool = pool;
	ctx->listen_sock = __fly_listen_sock(pool);
	if (ctx->listen_sock == NULL)
		return NULL;

	return ctx;
}

int fly_context_release(fly_context_t *ctx)
{
	return fly_delete_pool(&ctx->pool);
}

__fly_static fly_sockinfo_t *__fly_listen_sock(fly_pool_t *pool)
{
	fly_sockinfo_t *info;
	int port;
	const char *port_str;

	info = fly_pballoc(pool, sizeof(fly_sockinfo_t));
	if (!info)
		return NULL;

	port_str = fly_sockport_env();
	if (!port_str)
		return NULL;

	port = atoi(port_str);
	if (!port)
		return NULL;

	if (fly_socket_init(port, info) == -1)
		return NULL;

	return info;
}
