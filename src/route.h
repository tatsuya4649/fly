#ifndef _ROUTE_H
#define _ROUTE_H

//#include "request.h"
//#include "response.h"
#include "method.h"
#include "alloc.h"
#include "uri.h"
#include "err.h"

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
typedef struct fly_route fly_route_t;
typedef fly_response_t *fly_route_handler(fly_request_t *request, fly_route_t *route, void *data);

typedef const char fly_path;
struct fly_route{
	struct fly_route_reg	*reg;
	fly_route_handler		*function;
	struct fly_uri			*uri;
	fly_http_method_t		*method;
	fly_flag_t				flag;
	void					*data;
	struct fly_path_param	*path_param;

	struct fly_bllist		blelem;
};
#define FLY_ROUTE_FLAG_PYTHON			1UL << 5

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

#define FLY_REGISTER_ROUTE_SUCCESS							0
#define FLY_REGISTER_ROUTE_PATH_PARAM_NO_RIGHT_BRACKET		-1
#define FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR			-2
#define FLY_REGISTER_ROUTE_NO_METHOD						-3
int fly_register_route(fly_route_reg_t *reg, fly_route_handler *func, char *uri_path, size_t uri_size, fly_method_e method, fly_flag_t flag, void *data, struct fly_err *err);
fly_route_t *fly_found_route(fly_route_reg_t *reg, fly_uri_t *uri, fly_method_e method);
struct fly_http_method_chain *fly_valid_method(fly_pool_t *pool, fly_route_reg_t *reg, char *uri);

void fly_path_param_init(struct fly_path_param *__p);
int fly_parse_path_params_from_request(struct fly_request *req, fly_route_t *route);

static inline bool fly_is_noparam(struct fly_path_param *__pp){
	return __pp->param_count == 0 ? true: false;
}
#endif
