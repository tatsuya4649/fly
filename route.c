#include "route.h"
#include "alloc.h"

fly_pool_t *fly_route_pool = NULL;
int fly_route_init(void)
{
	if (fly_route_pool != NULL)
		return 0;
	fly_route_pool = fly_create_pool(FLY_ROUTE_POOL_PAGE);
	if (fly_route_pool == NULL)
		return -1;
	return 0;
}

fly_route_reg_t *fly_route_reg_init(void)
{
	fly_route_reg_t *reg;
	reg = fly_pballoc(fly_route_pool, sizeof(fly_route_reg_t));
	if (reg == NULL)
		return NULL;

	reg->pool = fly_route_pool;
	reg->regcount = 0;
	reg->entry = NULL;
	return reg;
}

int fly_route_release(void)
{
	if (fly_route_pool == NULL)
		return -1;

	return fly_delete_pool(fly_route_pool);
}
int fly_route_reg_release(fly_route_reg_t *reg)
{
	if (reg == NULL)
		return -1;

	return fly_delete_pool(reg->pool);
}

int fly_register_route(
	fly_route_reg_t *reg,
	fly_route *func,
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
