#include "route.h"
#include "alloc.h"
#include "context.h"
#include "char.h"
#include "request.h"

#define FLY_RR_PATH_PARAM_START				'{'
#define FLY_RR_PATH_PARAM_END				'}'

#define FLY_RR_PARSE_PATH_PARAM_SUCCESS					0
#define FLY_RR_PARSE_PATH_PARAM_NO_RIGHT_BRACKET		-1
#define FLY_RR_PARSE_PATH_PARAM_SYNTAX_ERROR			-2
#define FLY_RR_PARSE_PATH_PARAM_INTERNAL_ERROR			-3
static int fly_rr_parse_path_param(fly_route_reg_t *reg, fly_route_t *route, char *path, ssize_t path_size);
void fly_add_path_param(struct fly_path_param *path_param, struct fly_param *param);

fly_route_reg_t *fly_route_reg_init(fly_context_t *ctx)
{
	fly_route_reg_t *reg;

	reg = fly_pballoc(ctx->pool, sizeof(fly_route_reg_t));
	reg->pool = ctx->pool;
	reg->regcount = 0;
	fly_bllist_init(&reg->regs);
	return reg;
}

void fly_route_reg_release(fly_route_reg_t *reg)
{
	fly_pbfree(reg->pool, reg);
}

int fly_register_route(fly_route_reg_t *reg, fly_route_handler *func, char *uri_path, size_t uri_size, fly_method_e method, fly_flag_t flag, void *data, struct fly_err *err){
	fly_http_method_t *mtd;
	fly_route_t *route;
	fly_uri_t *uri;

#ifdef DEBUG
	assert(reg->pool != NULL);
#endif
	/* allocated register info */
	mtd = fly_match_method_type(method);
	if (fly_unlikely_null(mtd)){
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_ERR,
				"No such method %d",
				method
			);
		}
		return FLY_REGISTER_ROUTE_NO_METHOD;
	}

	route = fly_pballoc(reg->pool, sizeof(fly_route_t));
	uri = fly_pballoc(reg->pool, sizeof(fly_uri_t));
	uri->ptr = uri_path;
	uri->len = uri_size;

	route->function = func;
	route->uri = uri;
	route->method = mtd;
	route->flag = flag;
	route->reg = reg;
	route->data = data;
	route->path_param = NULL;

	switch(fly_rr_parse_path_param(reg, route, uri_path, uri_size)){
	case FLY_RR_PARSE_PATH_PARAM_SUCCESS:
		break;
	case FLY_RR_PARSE_PATH_PARAM_NO_RIGHT_BRACKET:
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_ERR,
				"No right brachet \"}\" error \"%s",
				uri_path
			);
		}
		fly_pbfree(reg->pool, uri);
		fly_pbfree(reg->pool, route);
		return FLY_REGISTER_ROUTE_PATH_PARAM_NO_RIGHT_BRACKET;
	case FLY_RR_PARSE_PATH_PARAM_SYNTAX_ERROR:
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_ERR,
				"Syntax error of path parameter \"%s",
				uri_path
			);
		}
		fly_pbfree(reg->pool, uri);
		fly_pbfree(reg->pool, route);
		return FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR;
	case FLY_RR_PARSE_PATH_PARAM_INTERNAL_ERROR:
		if (err != NULL){
			fly_error(
				err, errno,
				FLY_ERR_ERR,
				"Internal error of path parameter \"%s",
				uri_path
			);
		}
		fly_pbfree(reg->pool, uri);
		fly_pbfree(reg->pool, route);
		return FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR;
	default:
		FLY_NOT_COME_HERE
	}

	fly_bllist_add_tail(&reg->regs, &route->blelem);
	reg->regcount++;

	return FLY_REGISTER_ROUTE_SUCCESS;
}

/*
 *  fly path parameter:
 *		http://example.com/user/{ user_id: int }
 *								~~~~~~~~~~~~~~~~
 *	{ variable: type }
 *
 *	ABNF of variable:
 *		variable = 1*(alpha|us) *(alpha|us|digit)
 *		alpha = (%x41-5A|%x61-7A)
 *		digit = (%x30-39)
 *		us = %x5F
 *
 *
 *	type list:
 *	 * int
 *	 * float
 *	 * bool
 *	 * str
 */
#define FLY_PP_TYPE(__t)					(# __t)
static inline bool fly_pp_var_first(char c)
{
	return (fly_alpha(c) || fly_underscore(c)) ? true : false;
}
static inline bool fly_pp_var(char c)
{
	return (fly_alpha(c) || fly_numeral(c) || fly_underscore(c)) ? true : false;
}
static enum fly_path_param_type fly_pp_get_type(char **path, char *end)
{
	char *path_ptr;

	path_ptr = *path;
	if ((path_ptr + strlen(FLY_PP_TYPE(int))) < end && \
			strncmp(path_ptr, FLY_PP_TYPE(int), strlen(FLY_PP_TYPE(int))) == 0){
		*path += strlen(FLY_PP_TYPE(int));
		return FLY_PPT_INT;
	}else if ((path_ptr + strlen(FLY_PP_TYPE(float))) < end && \
			strncmp(path_ptr, FLY_PP_TYPE(float), strlen(FLY_PP_TYPE(float))) == 0){
		*path += strlen(FLY_PP_TYPE(int));
		return FLY_PPT_FLOAT;
	}else if ((path_ptr + strlen(FLY_PP_TYPE(bool))) < end && \
			strncmp(path_ptr, FLY_PP_TYPE(bool), strlen(FLY_PP_TYPE(bool))) == 0){
		*path += strlen(FLY_PP_TYPE(int));
		return FLY_PPT_FLOAT;
	}else if ((path_ptr + strlen(FLY_PP_TYPE(str))) < end && \
			strncmp(path_ptr, FLY_PP_TYPE(str), strlen(FLY_PP_TYPE(str))) == 0){
		*path += strlen(FLY_PP_TYPE(int));
		return FLY_PPT_STR;
	}

	return FLY_PPT_UNKNOWN;
}

static void __fly_release_path_param(fly_pool_t *pool, fly_route_t *route, struct fly_path_param * __pp)
{
	struct fly_bllist *__b, *__n;
	struct fly_param *__p;

	for (__b=__pp->params.next; __b!=&__pp->params; __b=__n){
		__n = __b->next;
		__p = fly_bllist_data(__b, struct fly_param, blelem);
		fly_pbfree(pool, __p);
	}
	route->path_param = NULL;
	fly_pbfree(pool, __pp);
	return;
}

static int fly_rr_parse_path_param(fly_route_reg_t *reg, fly_route_t *route, char *path, ssize_t path_size)
{
	struct fly_path_param *__pp;
	char *path_ptr=path;
	fly_pool_t *pool;

#ifdef DEBUG
	printf("Register Route Parse %s\n", path);
	assert(reg->pool != NULL);
#endif
	pool = reg->pool;
	__pp = fly_pballoc(pool, sizeof(struct fly_path_param));
	__pp->param_count = 0;
	fly_bllist_init(&__pp->params);
	route->path_param = __pp;

	while((path_ptr = memchr(path_ptr, FLY_RR_PATH_PARAM_START, path_size)) != NULL){
#ifdef DEBUG
		printf("Register Path Param %s\n", path_ptr);
#endif
		/* Syntax error */
		if (memchr(path_ptr, FLY_RR_PATH_PARAM_END, path_size) == NULL){
			__fly_release_path_param(pool, route, __pp);
			return FLY_RR_PARSE_PATH_PARAM_NO_RIGHT_BRACKET;
		}

		enum fly_ppt_status{
			INIT,
			START,
			SPACE,
			VAR,
			VAR_SPACE,
			COLON,
			TYPE,
			END,
		} status;
		struct fly_param *__fp;
		char *__p=path_ptr;
		char *var_ptr=NULL;
		size_t var_len=0;
		enum fly_path_param_type type;

		status = INIT;
		type = FLY_PPT_UNKNOWN;
		while(true){
			switch(status){
			case INIT:
				if (*__p == FLY_RR_PATH_PARAM_START){
					status = START;
					break;
				}
				goto syntax_error;
			case START:
				if (fly_space(*__p) || fly_ht(*__p)){
					status = SPACE;
					break;
				}
				if (fly_pp_var_first(*__p)){
					status = VAR;
					var_ptr = __p;
					continue;
				}
				goto syntax_error;
			case SPACE:
				if (fly_space(*__p) || fly_ht(*__p))
					break;
				if (fly_pp_var_first(*__p)){
					status = VAR;
					var_ptr = __p;
					continue;
				}
				goto syntax_error;
			case VAR:
				if (fly_pp_var_first(*__p))
					break;
				if (fly_pp_var(*__p))
					break;
				if (fly_space(*__p) || fly_ht(*__p)){
					var_len = __p - var_ptr;
					status = VAR_SPACE;
					break;
				}
				if (fly_colon(*__p)){
					var_len = __p - var_ptr;
					status = COLON;
					break;
				}
				if (*__p == FLY_RR_PATH_PARAM_END){
					var_len = __p - var_ptr;
					status = END;
					continue;
				}
				goto syntax_error;
			case VAR_SPACE:
				if (fly_space(*__p) || fly_ht(*__p))
					break;
				if (fly_colon(*__p)){
					status = COLON;
					break;
				}
				if (*__p == FLY_RR_PATH_PARAM_END){
					status = END;
					continue;
				}
				goto syntax_error;
			case COLON:
				if (fly_space(*__p) || fly_ht(*__p))
					break;
				if (fly_alpha(*__p)){
					status = TYPE;
					continue;
				}
				goto syntax_error;
			case TYPE:
				type = fly_pp_get_type(&__p, path+path_size);
				if (type == FLY_PPT_UNKNOWN)
					goto syntax_error;

				status = END;
				continue;
			case END:
				if (type == FLY_PPT_UNKNOWN)
					type = FLY_PPT_STR;

				if (var_ptr == NULL)
					goto error;
				if (var_len == 0)
					goto error;
				goto register_param;
			default:
				FLY_NOT_COME_HERE
			}

			if (__p >= path+path_size-1 && \
					(*__p !=  FLY_RR_PATH_PARAM_END))
				goto syntax_error;

			__p++;
			continue;
syntax_error:
			__fly_release_path_param(pool, route, __pp);
			return FLY_RR_PARSE_PATH_PARAM_SYNTAX_ERROR;
error:
			__fly_release_path_param(pool, route, __pp);
			return FLY_RR_PARSE_PATH_PARAM_INTERNAL_ERROR;
register_param:
#ifdef DEBUG
			printf("Register Path Parameter: varlen %ld: type %d: var %.*s\n", var_len, type, (int) var_len, var_ptr);
#endif
			__fp = fly_pballoc(pool, sizeof(struct fly_param));
			__fp->var_ptr = var_ptr;
			__fp->var_len = var_len;
			__fp->type = type;
			__fp->value = NULL;
			__fp->value_len = 0;
			__fp->param_number = __pp->param_count;
			fly_bllist_add_tail(&__pp->params, &__fp->blelem);
			__pp->param_count++;
			break;
		}
		path_ptr++;
	}
	return FLY_RR_PARSE_PATH_PARAM_SUCCESS;
}

static bool fly_match_param_uri(fly_route_t *__r, fly_uri_t *uri)
{
	char *uri_ptr, *__r_ptr;
	size_t uri_len = uri->len, __r_len = __r->uri->len;
	size_t __j=0;

	uri_ptr = uri->ptr;
	__r_ptr = __r->uri->ptr;

	for (size_t __i=0; __i < uri_len; __i++, __j++){
		if (__r_ptr[__j] == FLY_RR_PATH_PARAM_START){
			char *__rslash, *urislash;

			__rslash = memchr(__r_ptr+__j, '/', __r_len-__j);
			urislash = memchr(uri_ptr+__i, '/', uri_len-__i);
			/*
			 *	Parse: { var: type }
			 */

			/*
			 *	Case1:
			 *		/user/{ user_id: int }
			 *							  ~
			 *		/user/10/
			 *				~
			 *				Not Match!!!
			 */
			if (__rslash == NULL && urislash != NULL)
				return false;
			/*
			 *	Case2:
			 *		/user/{ user_id: int }/
			 *							  ~
			 *		/user/10
			 *				~
			 *				Not Match!!!
			 */
			if (__rslash != NULL && urislash == NULL)
				return false;
			/*
			 *  Case3:
			 *		/user/{ user_id: int}
			 *			  ~~~~~~~~~~~~~~~
			 *		/user/10
			 *			  ~~
			 *			  Matching!!!
			 */
			if (__rslash == NULL && urislash == NULL)
				return true;

			/*
			 *	 Jump to next path param
			 *
			 *	 /user/10000/post
			 *		   ---->
			 *
			 *	 /user/{ user_id: int }/post
			 *	       --------------->
			 */
			__i += (urislash-uri_ptr);
			while (__r_ptr[__j] != FLY_RR_PATH_PARAM_END)
				__j++;
		}
		/* Not match  */
		if (uri_ptr[__i] != __r_ptr[__j])
			return false;
	}

	if (__j == __r_len)
		return true;
	else
		return false;
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
#ifdef DEBUG
		assert(__r->path_param);
#endif
		if (fly_is_noparam(__r->path_param)){
			if ((__r->uri->len == uri->len) && \
					(strncmp(__r->uri->ptr, uri->ptr, uri->len) == 0) && \
					(__r->method->type==method))
				return __r;
		}else{
			if (fly_match_param_uri(__r, uri))
				return __r;
		}
	}
	return NULL;
}

struct fly_http_method_chain *fly_valid_method(fly_pool_t *pool, fly_route_reg_t *reg, char *uri)
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
		if (__r->path_param != NULL || fly_path_param_count(__r->path_param) == 0){
			if (strncmp(__r->uri->ptr, uri, __r->uri->len) == 0 && \
					__r->method->type != GET){
				struct fly_http_method *__nc;
				__nc = fly_pballoc(pool, sizeof(struct fly_http_method));
				__nc->name = __r->method->name;
				__nc->type = __r->method->type;
				fly_bllist_add_tail(&__mc->method_chain, &__nc->blelem);
				__mc->chain_length++;
			}
		}else{
			/* TODO: path patameter */
		}
	}
	return __mc;
}

void fly_path_param_init(struct fly_path_param *__p)
{
	__p->param_count = 0;
	fly_bllist_init(&__p->params);
	return;
}

struct fly_param *fly_path_param_from_number(struct fly_path_param *__p, int number)
{
	struct fly_bllist *__b;
	struct fly_param *__pp;

	fly_for_each_bllist(__b, &__p->params){
		__pp = fly_bllist_data(__b, struct fly_param, blelem);
		if (__pp->param_number == number)
			return __pp;
	}
#ifdef DEBUG
	FLY_NOT_COME_HERE
#endif
	return NULL;
}

int fly_parse_path_params_from_request(struct fly_request *req, fly_route_t *route)
{
#ifdef DEBUG
	assert(req->request_line != NULL);
#endif
	/* No path parameter */
	if (fly_path_param_count(route->path_param) == 0)
		return 0;

	fly_uri_t *uri;
	char *uri_ptr, *__r_ptr;
	size_t __j=0;
	struct fly_request_line *reqline;
	int param_number = 0;

	reqline = req->request_line;
	uri = &reqline->uri;
	uri_ptr = uri->ptr;
	__r_ptr = route->uri->ptr;

	for (size_t __i=0; __i < uri->len; __i++, __j++){
		if (__r_ptr[__j] == FLY_RR_PATH_PARAM_START){
			struct fly_param *__p, *reg_param;

			__p = fly_pballoc(req->pool, sizeof(struct fly_param));
			reg_param = fly_path_param_from_number(route->path_param, param_number);
			if (reg_param == NULL)
				return -1;

			memcpy(__p, reg_param, sizeof(struct fly_param));

			/* parse parameter */
			__p->value = (uint8_t *) uri_ptr+__i;
			char *__next = memchr(uri_ptr+__i, FLY_SLASH, \
						(uri->ptr+uri->len)-(uri_ptr+__i));
			if (__next == NULL)
				__p->value_len = (uri->ptr+uri->len)-(uri_ptr+__i);
			else
				__p->value_len = __next-(uri_ptr+__i);

			fly_add_path_param(&reqline->path_params, __p);
			param_number++;

			if (__next == NULL)
				return 0;

			__i += (__p->value_len);
			char *__rnext;

			/*
			 *	 /user/{ user_id: int}/path...
			 *		   -------------->
			 *		   Jump next slash
			 */
			__rnext = memchr(__r_ptr+__j, FLY_SLASH, \
					(route->uri->ptr+route->uri->len)-(__r_ptr+__j));
#ifdef DEBUG
			assert(__rnext != NULL);
#endif
			__j += (__rnext-(__r_ptr+__j));
		}
#ifdef DEBUG
		assert(uri_ptr[__i] == __r_ptr[__j]);
#endif
	}
	return 0;
}

void fly_add_path_param(struct fly_path_param *path_param, struct fly_param *param)
{
	fly_bllist_add_tail(&path_param->params, &param->blelem);
	path_param->param_count++;
}
