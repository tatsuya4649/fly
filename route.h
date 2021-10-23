#ifndef _ROUTE_H
#define _ROUTE_H

//#include "request.h"
//#include "response.h"
#include "method.h"
#include "alloc.h"

/*
 @params:
	request: all of request information.
 @return:
	success: 0
	failure: -1
 */
typedef struct fly_response fly_response_t;
typedef struct fly_request fly_request_t;
typedef unsigned long long fly_flag_t;
typedef fly_response_t *fly_route_handler(fly_request_t *request, void *data);

typedef const char fly_path;
struct fly_route{
	struct fly_route_reg	*reg;
	fly_route_handler		*function;
	fly_path				*uri;
	fly_http_method_t		*method;
	fly_flag_t				flag;
	void					*data;

	struct fly_bllist		blelem;
};
#define FLY_ROUTE_FLAG_PYTHON			1UL << 5
typedef struct fly_route fly_route_t;

#define FLY_ROUTEREG_POOL_PAGE			100
#define FLY_ROUTEREG_POOL_SIZE			(FLY_PAGESIZE*FLY_ROUTEREG_POOL_PAGE)
//extern fly_pool_t *fly_route_pool;

typedef struct __fly_route __fly_route_t;
struct fly_route_reg{
	fly_pool_t *pool;
	unsigned regcount;

	struct fly_bllist regs;
};
typedef struct fly_route_reg fly_route_reg_t;

void fly_route_reg_release(fly_route_reg_t *reg);
struct fly_context;
fly_route_reg_t *fly_route_reg_init(struct fly_context *ctx);

int fly_register_route(fly_route_reg_t *reg, fly_route_handler *func, fly_path *uri, fly_method_e method, fly_flag_t flag, void *data);
fly_route_t *fly_found_route(fly_route_reg_t *reg, fly_path *path, fly_method_e method);
struct fly_http_method_chain *fly_valid_method(fly_pool_t *pool, fly_route_reg_t *reg, fly_path *uri);


#endif
