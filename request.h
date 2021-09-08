#ifndef _REQUEST_H
#define _REQUEST_H

#include <string.h>
#include <errno.h>
#include "connect.h"
#include "alloc.h"
#include "method.h"
#include "version.h"
#include "header.h"
#include "body.h"
#include "server.h"
#include "uri.h"
#include "util.h"

#define FLY_BUFSIZE			(8*FLY_PAGESIZE)
#define FLY_REQUEST_LINE_MAX			8000
#define FLY_REQUEST_URI_MAX				6000

typedef char fly_reqlinec_t;
typedef char fly_buffer_t;
struct fly_request_line{
	fly_reqlinec_t *request_line;
	fly_http_method_t *method;
	fly_http_uri_t uri;
	fly_http_version_t *version;
};
typedef struct fly_request_line fly_reqline_t;

struct fly_request{
	fly_pool_t *pool;
	fly_connect_t *connect;
	fly_reqline_t *request_line;
	fly_hdr_ci *header;
	fly_body_t *body;
	fly_buffer_t *buffer;
};
typedef struct fly_request fly_request_t;

int fly_request_receive(fly_sock_t fd, fly_request_t *request);
int fly_request_event_handler(fly_event_t *event);

int fly_request_operation(int c_sock, fly_pool_t *pool,fly_reqlinec_t *request_line, fly_request_t *req);
int fly_reqheader_operation(fly_request_t *req, fly_buffer_t *header);

#define FLY_REQUEST_POOL_SIZE		1
fly_request_t *fly_request_init(fly_connect_t *conn);
int fly_request_release(fly_request_t *req);

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer);
#endif
