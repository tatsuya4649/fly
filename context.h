#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "alloc.h"
#include "server.h"
#include "log.h"
#include "route.h"

#define FLY_CONTEXT_POOL_SIZE			10


struct fly_response_content_by_stcode;
struct fly_context{
	fly_pool_t *pool;
	fly_pool_t *misc_pool;
	fly_sockinfo_t *listen_sock;
	fly_sockinfo_t *dummy_sock;
	int listen_count;
	fly_log_t *log;
	fly_route_reg_t *route_reg;
	fly_mount_t *mount;
	struct fly_response_content_by_stcode *rcbs;
};
typedef struct fly_context fly_context_t;

#define FLY_DUMMY_SOCK_INIT(ctx)			\
	{										\
		(ctx)->dummy_sock = fly_pballoc((ctx)->pool, sizeof(fly_sockinfo_t));	\
		if (fly_unlikely_null((ctx)->dummy_sock))		\
			return NULL;								\
		(ctx)->dummy_sock->next = (ctx)->dummy_sock;	\
	}

fly_context_t *fly_context_init(void);
int fly_context_release(fly_context_t *ctx);

#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_SUCCESS		(1)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_BLOCKING		(0)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_NOTFOUND		(-1)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR		(-2)
enum status_code_type;
int is_fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code);
int fly_send_default_content_by_stcode(fly_event_t *e, enum status_code_type status_code);
struct fly_response_content_by_stcode *fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code);
struct fly_response_content_by_stcode *fly_default_content_by_stcode_from_event(fly_event_t *e, enum status_code_type status_code);
int fly_send_default_content(fly_event_t *e, struct fly_response_content_by_stcode *__r);
#define FLY_SEND_BUF_LENGTH			(4096)

#endif
