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
typedef fly_response_t *fly_route_handler(fly_request_t *request);

/**/
typedef const char fly_path;
struct fly_route{
	fly_route_handler *function;
	fly_path *uri;
	fly_http_method_t method;
	fly_flag_t flag;
};
#define FLY_ROUTE_FLAG_PYTHON			1UL << 5
typedef struct fly_route fly_route_t;

#define FLY_ROUTEREG_POOL_PAGE			100
#define FLY_ROUTEREG_POOL_SIZE			(FLY_PAGESIZE*FLY_ROUTEREG_POOL_PAGE)
//extern fly_pool_t *fly_route_pool;

struct __fly_route{
	struct __fly_route *next;
	fly_route_t *route;
};
typedef struct __fly_route __fly_route_t;
struct fly_route_reg{
	fly_pool_t *pool;
	unsigned regcount;
	__fly_route_t *entry;
	__fly_route_t *last;
};
typedef struct fly_route_reg fly_route_reg_t;

//int fly_route_init(void);
//int fly_route_release(void);
int fly_route_reg_release(fly_route_reg_t *reg);
fly_route_reg_t *fly_route_reg_init(void);

int fly_register_route(fly_route_reg_t *reg, fly_route_handler *func, fly_path *uri, fly_method_e method, fly_flag_t flag);
fly_route_t *fly_found_route(fly_route_reg_t *reg, fly_path *path, fly_method_e method);

#endif
