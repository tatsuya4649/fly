#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "alloc.h"
#include "server.h"
#include "log.h"
#include "route.h"
#include "ssl.h"
#include "bllist.h"

#define FLY_CONTEXT_POOL_SIZE			10

struct fly_response_content_by_stcode;
struct fly_context{
	fly_pool_t					*pool;
	fly_pool_t					*event_pool;
	struct fly_pool_manager		*pool_manager;
	fly_pool_t					*misc_pool;
	fly_sockinfo_t				*listen_sock;
	int							listen_count;
	fly_log_t					*log;
	fly_route_reg_t				*route_reg;
	fly_mount_t					*mount;
	struct fly_bllist			rcbs;

	void						*data;
	long long					max_response_content_length;
	size_t						max_request_length;
	size_t						response_encode_threshold;
	int							request_timeout;
	bool						log_stdout;
	bool						log_stderr;
	/* for SSL/TLS */
	SSL_CTX						*ssl_ctx;

	fly_bit_t					daemon;
};
typedef struct fly_context fly_context_t;

fly_context_t *fly_context_init(struct fly_pool_manager *__pm);
void fly_context_release(fly_context_t *ctx);

#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_SUCCESS		(1)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_BLOCKING		(0)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_NOTFOUND		(-1)
#define FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR		(-2)
enum status_code_type;
int is_fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code);
int fly_send_default_content_by_stcode(fly_event_t *e, enum status_code_type status_code);
struct fly_response_content_by_stcode *fly_default_content_by_stcode(fly_context_t *ctx, enum status_code_type status_code);
struct fly_response_content_by_stcode *fly_default_content_by_stcode_from_event(fly_event_t *e, enum status_code_type status_code);
//int fly_send_default_content(fly_event_t *e, struct fly_response_content_by_stcode *__r);
#define FLY_SEND_BUF_LENGTH			(4096)

__unused static inline bool is_fly_log_fd(int i, fly_context_t *ctx){
	if (i == ctx->log->access->file)
		return true;
	else if (i == ctx->log->error->file)
		return true;
	else if (i == ctx->log->notice->file)
		return true;
	else
		return false;

	FLY_NOT_COME_HERE
}

__unused static inline bool is_fly_listen_socket(int i, fly_context_t *ctx){
	if (i == ctx->listen_sock->fd)
		return true;
	else
		return false;
}

#endif
