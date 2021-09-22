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

#define FLY_BUFSIZE			(8*FLY_PAGESIZE)
#define FLY_REQUEST_LINE_MAX			8000
#define FLY_REQUEST_URI_MAX				6000
#define FLY_REQUEST_TIMEOUT				(60)
#define FLY_REQUEST_NOREADY				100

struct fly_query{
	char *ptr;
	size_t len;
};
typedef struct fly_query fly_query_t;

typedef char fly_reqlinec_t;
typedef char fly_buffer_t;
struct fly_request_line{
	fly_reqlinec_t *request_line;
	fly_http_method_t *method;
	fly_uri_t uri;
	fly_query_t query;
	fly_http_version_t *version;
};
typedef struct fly_request_line fly_reqline_t;


enum fly_request_fase{
	EFLY_REQUEST_FASE_INIT,
	EFLY_REQUEST_FASE_REQUEST_LINE,
	EFLY_REQUEST_FASE_HEADER,
	EFLY_REQUEST_FASE_BODY,
	EFLY_REQUEST_FASE_RESPONSE,
};
#define fly_event_fase(e, fase)			((e)->event_fase = (void *) EFLY_REQUEST_FASE_ ## fase)
typedef enum fly_request_fase fly_request_fase_t;
enum fly_request_state{
	EFLY_REQUEST_STATE_INIT,
	EFLY_REQUEST_STATE_RECEIVE,
	EFLY_REQUEST_STATE_CONT,
	EFLY_REQUEST_STATE_RESPONSE,
	EFLY_REQUEST_STATE_END,
	EFLY_REQUEST_STATE_TIMEOUT,
};
#define fly_event_state(e, event)		((e)->event_state = (void *) EFLY_REQUEST_STATE_ ## event)
typedef enum fly_request_state fly_request_state_t;
#include "encode.h"
#include "mime.h"
#include "charset.h"
#include "lang.h"
struct fly_request{
	/* use pool: request, connect, header, body */
	fly_context_t       *ctx;
	fly_pool_t			*pool;
	fly_connect_t		*connect;
	fly_reqline_t		*request_line;
	fly_hdr_ci			*header;
	fly_body_t			*body;
	fly_buffer_t		*buffer;
	fly_buffer_t		*bptr;
	fly_request_fase_t	 fase;
	fly_encoding_t		*encoding;
	fly_mime_t			*mime;
	fly_charset_t		*charset;
	fly_lang_t			*language;
};
typedef struct fly_request fly_request_t;

int fly_request_receive(fly_sock_t fd, fly_request_t *request);
int fly_request_event_handler(fly_event_t *event);

int fly_reqheader_operation(fly_request_t *req, fly_buffer_t *header);

#define FLY_REQUEST_POOL_SIZE		1
fly_request_t *fly_request_init(fly_connect_t *conn);
int fly_request_release(fly_request_t *req);

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer);
int fly_request_timeout(fly_event_t *event);
#endif
