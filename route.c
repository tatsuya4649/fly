#include "route.h"
#include "alloc.h"
#include "context.h"

fly_route_reg_t *fly_route_reg_init(fly_context_t *ctx)
{
	fly_route_reg_t *reg;

	reg = fly_pballoc(ctx->pool, sizeof(fly_route_reg_t));
	if (reg == NULL)
		return NULL;

	reg->pool = ctx->pool;
	reg->regcount = 0;
	fly_bllist_init(&reg->regs);
	return reg;
}

void fly_route_reg_release(fly_route_reg_t *reg)
{
	fly_pbfree(reg->pool, reg);
}

int fly_register_route(fly_route_reg_t *reg, fly_route_handler *func, fly_path *uri, fly_method_e method, fly_flag_t flag, void *data){
	fly_http_method_t *mtd;
	fly_route_t *route;

#ifdef DEBUG
	assert(reg->pool != NULL);
#endif
	/* allocated register info */
	mtd = fly_match_method_type(method);
	if (fly_unlikely_null(mtd))
		return -1;

	route = fly_pballoc(reg->pool, sizeof(fly_route_t));
	if (fly_unlikely_null(route))
		return -1;

	route->function = func;
	route->uri = uri;
	route->method = mtd;
	route->flag = flag;
	route->reg = reg;
	route->data = data;

	fly_bllist_add_tail(&reg->regs, &route->blelem);
	reg->regcount++;

	return 0;
}

fly_route_t *fly_found_route(fly_route_reg_t *reg, fly_uri_t *uri, fly_method_e method)
{
	struct fly_bllist *__b;
	fly_route_t *__r;
#if DEBUG
	assert(reg != NULL);
#endif
	fly_for_each_bllist(__b, &reg->regs){
		__r = fly_bllist_data(__b, fly_route_t, blelem);
		if ((strlen(__r->uri) == uri->len) && \
				(strncmp(__r->uri, uri->ptr, uri->len) == 0) && \
				(__r->method->type==method))
			return __r;
	}
	return NULL;
}

struct fly_http_method_chain *fly_valid_method(fly_pool_t *pool, fly_route_reg_t *reg, fly_path *uri)
{
	struct fly_http_method_chain *__mc;

#ifdef DEBUG
	assert(reg != NULL);
#endif

	struct fly_bllist *__b;
	fly_route_t *__r;

	__mc = fly_pballoc(pool, sizeof(struct fly_http_method_chain));
	if (fly_unlikely_null(__mc))
		return NULL;
	__mc->chain_length = 0;
	fly_bllist_init(&__mc->method_chain);

	struct fly_http_method *__gc, *__get;
	__gc = fly_pballoc(pool, sizeof(struct fly_http_method));
	if (fly_unlikely_null(__gc))
		return NULL;
	__get = fly_match_method_type(GET);
	__gc->name = __get->name;
	__gc->type = __get->type;
	fly_bllist_add_tail(&__mc->method_chain, &__gc->blelem);
	__mc->chain_length++;

	fly_for_each_bllist(__b, &reg->regs){
		__r = fly_bllist_data(__b, fly_route_t, blelem);
		if (strncmp(__r->uri, uri, strlen(__r->uri)) == 0 && \
				__r->method->type != GET){
			struct fly_http_method *__nc;
			__nc = fly_pballoc(pool, sizeof(struct fly_http_method));
			if (fly_unlikely_null(__nc))
				return NULL;
			__nc->name = __r->method->name;
			__nc->type = __r->method->type;
			fly_bllist_add_tail(&__mc->method_chain, &__nc->blelem);
			__mc->chain_length++;
		}
	}
	return __mc;
}
