#ifndef _REQUEST_H
#define _REQUEST_H

#include <string.h>
#include <stdbool.h>
#include <errno.h>
#include "context.h"
#include "connect.h"
#include "alloc.h"
#include "method.h"
#include "version.h"
#include "header.h"
#include "body.h"
#include "server.h"
#include "route.h"
#include "uri.h"
#include "util.h"
#include "err.h"
#include "mime.h"
#include "buffer.h"
#include "scheme.h"

#define FLY_REQUEST_LINE_MAX			8000
#define FLY_REQUEST_URI_MAX				6000
#define FLY_REQUEST_NOREADY				100
#define FLY_REQUEST_TIMEOUT				"FLY_REQUEST_TIMEOUT"

struct fly_query{
	char *ptr;
	size_t len;
};
#define fly_request_query_init(__q)			\
	do{	\
		(__q)->ptr = NULL;					\
		(__q)->len = 0;						\
	} while(0)
typedef struct fly_query fly_query_t;

typedef char fly_reqlinec_t;
struct fly_request_line{
	fly_reqlinec_t			*request_line;
	size_t					request_line_len;
	fly_http_method_t		*method;
	fly_uri_t				uri;
	fly_query_t				query;
	fly_http_version_t		*version;
	fly_scheme_t			*scheme;
};
#define is_fly_request_http_v2(req)		((req)->request_line->version->type == V2)
typedef struct fly_request_line fly_reqline_t;


enum fly_request_fase{
	EFLY_REQUEST_FASE_INIT,
	EFLY_REQUEST_FASE_REQUEST_LINE,
	EFLY_REQUEST_FASE_HEADER,
	EFLY_REQUEST_FASE_BODY,
	EFLY_REQUEST_FASE_RESPONSE,
};
#define fly_event_request_fase(e, fase)			\
	fly_event_fase_set((e), __e, EFLY_REQUEST_FASE_ ## fase)
	//((e)->event_fase = (void *) EFLY_REQUEST_FASE_ ## fase)
typedef enum fly_request_fase fly_request_fase_t;
enum fly_request_state{
	EFLY_REQUEST_STATE_INIT,
	EFLY_REQUEST_STATE_RECEIVE,
	EFLY_REQUEST_STATE_CONT,
	EFLY_REQUEST_STATE_RESPONSE,
	EFLY_REQUEST_STATE_END,
	EFLY_REQUEST_STATE_TIMEOUT,
};
#define fly_event_request_state(e, event)		\
	fly_event_state_set((e), __e, EFLY_REQUEST_STATE_ ## event)
	//((e)->event_state = (void *) EFLY_REQUEST_STATE_ ## event)
typedef enum fly_request_state fly_request_state_t;
#include "encode.h"
#include "mime.h"
#include "charset.h"
#include "lang.h"
struct fly_hv2_stream;
typedef struct fly_hv2_stream fly_hv2_stream_t;

struct fly_request{
	/* use pool: request, connect, header, body */
	fly_context_t       *ctx;
	fly_pool_t			*pool;
	fly_connect_t		*connect;
	fly_reqline_t		*request_line;
	fly_hdr_ci			*header;
	fly_body_t			*body;
	fly_buffer_t		*buffer;
	fly_buffer_c		*bptr;
	fly_request_fase_t	 fase;
	fly_encoding_t		*encoding;
	fly_mime_t			*mime;
	fly_charset_t		*charset;
	fly_lang_t			*language;
	size_t				discard_length;

	/* for http2 */
	fly_hv2_stream_t		*stream;

	fly_bit_t			receive_status_line:1;
	fly_bit_t			receive_header:1;
	fly_bit_t			receive_body:1;
	fly_bit_t			discard_body:1;
};
typedef struct fly_request fly_request_t;

#define FLY_CONNECT_ON_SSL(c)		\
	((c)->flag & FLY_SSL_CONNECT)
#define FLY_SSL_FROM_REQUEST(r)		\
	((r)->connect->ssl)

#define FLY_REQUEST_BUFFER_CHAIN_INIT_LEN			(1)
#define FLY_REQUEST_BUFFER_CHAIN_INIT_CHAIN_MAX		(100)
#define FLY_REQUEST_BUFFER_CHAIN_INIT_PER_LEN		(10)

#define FLY_REQUEST_RECEIVE_ERROR				(-1)
#define FLY_REQUEST_RECEIVE_SUCCESS				(1)
#define FLY_REQUEST_RECEIVE_END					(0)
#define FLY_REQUEST_RECEIVE_READ_BLOCKING		(2)
#define FLY_REQUEST_RECEIVE_WRITE_BLOCKING		(3)
#define FLY_REQUEST_RECEIVE_OVERFLOW			(4)
int fly_request_event_handler(fly_event_t *event);

#define FLY_REQUEST_POOL_SIZE		1
fly_request_t *fly_request_init(fly_connect_t *conn);
void fly_request_release(fly_request_t *req);

void fly_request_line_init(fly_request_t *req);
void fly_request_line_release(fly_request_t *req);

struct fly_buffer_chain *fly_get_request_line_ptr(fly_buffer_t *__buf);
int fly_request_timeout_handler(fly_event_t *event);
int fly_hv2_request_target_parse(fly_request_t *req);
int fly_if_none_match(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);
int fly_if_modified_since(fly_hdr_ci *ci, struct fly_mount_parts_file *pf);
int fly_request_timeout(void);
int fly_request_fail_close_handler(fly_event_t *event, int fd __fly_unused);

static inline void fly_query_set(fly_request_t *req, fly_reqlinec_t *c, size_t len)
{
	req->request_line->query.ptr = c;
	req->request_line->query.len = len;
	return;
}

#endif
