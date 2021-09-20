#include "route.h"
#include "alloc.h"

fly_route_reg_t *fly_route_reg_init(void)
{
	fly_pool_t *pool;
	fly_route_reg_t *reg;

	pool = fly_create_pool(FLY_ROUTEREG_POOL_SIZE);
	if (pool == NULL)
		return NULL;

	reg = fly_pballoc(pool, sizeof(fly_route_reg_t));
	if (reg == NULL)
		return NULL;

	reg->pool = pool;
	reg->regcount = 0;
	reg->entry = NULL;
	return reg;
}

int fly_route_reg_release(fly_route_reg_t *reg)
{
	if (reg == NULL)
		return -1;

	return fly_delete_pool(&reg->pool);
}

int fly_register_route(
	fly_route_reg_t *reg,
	fly_route_handler *func,
	fly_path *uri,
	fly_method_e method,
	fly_flag_t flag
){
	fly_http_method_t *mtd;
	fly_route_t *route;
	__fly_route_t *__route;

	if (reg->pool == NULL)
		return -1;
	/* allocated register info */
	mtd = fly_match_method_type(method);
	if (mtd == NULL)
		return -1;

	route = fly_pballoc(reg->pool, sizeof(fly_route_t));
	if (route == NULL)
		return -1;

	route->function = func;
	route->uri = uri;
	route->method = *mtd;
	route->flag = flag;

	/* allocated wrapper register info */
	__route = fly_pballoc(reg->pool, sizeof(__fly_route_t));
	if (__route == NULL)
		return -1;
	__route->next = NULL;
	__route->route = route;

	if (reg->entry == NULL)
		reg->entry = __route;
	else
		reg->last->next = __route;
	reg->last = __route;
	reg->regcount++;

	return 0;
}

fly_route_t *fly_found_route(fly_route_reg_t *reg, fly_path *uri, fly_method_e method)
{
	if (reg == NULL || reg->entry == NULL)
		return NULL;

	for(__fly_route_t *r=reg->entry; r!=NULL; r=r->next){
		if (strcmp(r->route->uri, uri) == 0 && r->route->method.type==method)
			return r->route;
	}
	return NULL;
}

__fly_static void __fly_connect_method_chain(struct fly_http_method_chain *base, struct fly_http_method_chain *nc)
{
	struct fly_http_method_chain *__c;
	for (__c=base; __c->next; __c=__c->next)
		;
	__c->next = nc;
	nc->next = NULL;
	base->chain_length++;
	nc->chain_length = base->chain_length;
	return;
}

struct fly_http_method_chain *fly_valid_method(fly_pool_t *pool, fly_route_reg_t *reg, fly_path *uri)
{
	struct fly_http_method_chain *__c;

	__c = fly_pballoc(pool, sizeof(struct fly_http_method_chain));
	if (fly_unlikely_null(__c))
		return NULL;
	__c->method = fly_match_method_type(GET);
	__c->chain_length = 1;
	__c->next = NULL;

	if (reg == NULL || reg->entry == NULL)
		return __c;

	for (__fly_route_t *__r=reg->entry; __r; __r=__r->next){
		if (strcmp(__r->route->uri, uri) == 0){
			struct fly_http_method_chain *__nc;
			__nc = fly_pballoc(pool, sizeof(struct fly_http_method_chain));
			if (fly_unlikely_null(__nc))
				return NULL;
			__nc->method = &__r->route->method;
			__nc->next = NULL;
			__fly_connect_method_chain(__c, __nc);
		}
	}
	return __c;
}
