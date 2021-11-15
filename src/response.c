#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "request.h"
#include "header.h"
#include "alloc.h"
#include "mount.h"
#include "math.h"
#include "v2.h"
#include "encode.h"
#include "response.h"

fly_status_code responses[] = {
	/* 1xx Info */
	{FLY_STATUS_CODE(100), "Continue", NULL,				NULL},
	{FLY_STATUS_CODE(101), "Switching Protocols", NULL,			FLY_STRING_ARRAY("Upgrade", NULL)},
	/* 2xx Success */
	{FLY_STATUS_CODE(200), "OK", NULL, 					NULL},
	{FLY_STATUS_CODE(201), "Created", NULL,				NULL},
	{FLY_STATUS_CODE(202), "Accepted", NULL, 				NULL},
	{FLY_STATUS_CODE(203), "Non-Authoritative Information", NULL, NULL},
	{FLY_STATUS_CODE(204), "No Content", NULL, 					NULL},
	{FLY_STATUS_CODE(205), "Reset Content", NULL,				NULL},
	/* 3xx Redirect */
	{FLY_STATUS_CODE(300), "Multiple Choices", NULL, 				NULL},
	{FLY_STATUS_CODE(301), "Moved Permanently", NULL,			FLY_STRING_ARRAY("Location", NULL)},
	{FLY_STATUS_CODE(302), "Found", NULL,						FLY_STRING_ARRAY("Location", NULL)},
	{FLY_STATUS_CODE(303), "See Other", NULL,					FLY_STRING_ARRAY("Location", NULL)},
	{FLY_STATUS_CODE(304), "Not Modified",	NULL,				NULL},
	{FLY_STATUS_CODE(307), "Temporary Redirect", NULL,			FLY_STRING_ARRAY("Location", NULL)},
	/* 4xx Client Error */
	{FLY_STATUS_CODE(400), "Bad Request", NULL,					NULL},
	{FLY_STATUS_CODE(401), "Unauthorized",	NULL,				NULL},
	{FLY_STATUS_CODE(402), "Payment Required", NULL,				NULL},
	{FLY_STATUS_CODE(403), "Forbidden", NULL,					NULL},
	{FLY_STATUS_CODE(404), "Not Found", FLY_PATH_FROM_STATIC(404.html),	NULL},
	{FLY_STATUS_CODE(405), "Method Not Allowed", FLY_PATH_FROM_STATIC(405.html),			FLY_STRING_ARRAY("Allow", NULL)},
	{FLY_STATUS_CODE(406), "Not Acceptable", NULL,				NULL},
	{FLY_STATUS_CODE(407), "Proxy Authentication Required", NULL, NULL},
	{FLY_STATUS_CODE(408), "Request Timeout", NULL,				FLY_STRING_ARRAY("Connection", NULL)},
	{FLY_STATUS_CODE(409), "Conflict", NULL,						NULL},
	{FLY_STATUS_CODE(409), "Gone", NULL,							NULL},
	{FLY_STATUS_CODE(410), "Length Required", NULL,				NULL},
	{FLY_STATUS_CODE(413), "Payload Too Large", NULL,			FLY_STRING_ARRAY("Retry-After", NULL)},
	{FLY_STATUS_CODE(414), "URI Too Long", NULL,					NULL},
	{FLY_STATUS_CODE(415), "Unsupported Media Type", NULL,		NULL},
	{FLY_STATUS_CODE(416), "Range Not Satisfiable", NULL,		NULL},
	{FLY_STATUS_CODE(417), "Expectation Failed", NULL,			NULL},
	{FLY_STATUS_CODE(426), "Upgrade Required", NULL,				FLY_STRING_ARRAY("Upgrade", NULL)},
	/* 5xx Server Error */
	{FLY_STATUS_CODE(500), "Internal Server Error", NULL,		NULL},
	{FLY_STATUS_CODE(501), "Not Implemented", NULL,				NULL},
	{FLY_STATUS_CODE(502), "Bad Gateway", NULL,			NULL},
	{FLY_STATUS_CODE(503), "Service Unavailable", NULL,		NULL},
	{FLY_STATUS_CODE(504), "Gateway Timeout", NULL,				NULL},
	{FLY_STATUS_CODE(505), "HTTP Version Not SUpported", NULL,	NULL},
	{-1, -1, NULL, NULL, NULL, NULL}
};

__fly_static int __fly_response_release_handler(fly_event_t *e);
#define FLY_RESPONSE_LOG_ITM_FLAG		(1)
int fly_response_log(fly_response_t *res, fly_event_t *e);
__fly_static int __fly_response_logcontent(fly_response_t *response, fly_event_t *e, fly_logcont_t *lc);
__fly_static int fly_after_response(fly_event_t *e, fly_response_t *response);

fly_response_t *fly_response_init(struct fly_context *ctx)
{
	fly_response_t *response;
	fly_pool_t *pool;
	pool = fly_create_pool(ctx->pool_manager, FLY_RESPONSE_POOL_PAGE);
	if (pool == NULL)
		return NULL;

	response = fly_pballoc(pool, sizeof(fly_response_t));
	response->pool = pool;
	response->header = NULL;
	response->body = NULL;
	response->request = NULL;
	response->fase = FLY_RESPONSE_READY;
	response->send_ptr = NULL;
	response->byte_from_start = 0;
	response->pf = NULL;
	response->offset = 0;
	response->count = 0;
	response->encoding_type = NULL;
	response->de = NULL;
	response->blocking = false;
	response->encoded = false;
	response->rcbs = NULL;
	response->send_len = 0;
	response->response_len = 0;
	response->original_response_len = 0;
	return response;
}

struct fly_response_content_by_stcode *fly_rcbs_init(fly_context_t *ctx)
{
	struct fly_response_content_by_stcode *__frc;

	__frc = fly_pballoc(ctx->pool, sizeof(struct fly_response_content_by_stcode));
	if (fly_unlikely_null(__frc))
		return NULL;
	memset(__frc->content_path, '\0', FLY_PATH_MAX);
	__frc->fd = -1;
	__frc->mime = NULL;
	__frc->de = NULL;
	__frc->encode_type = FLY_RCBS_DEFAULT_ENCODE_TYPE;
	__frc->encoded = false;
	return __frc;
}

__fly_static char **__fly_stcode_required_header(fly_stcode_t type)
{
	for (fly_status_code *res=responses; res->status_code!=-1; res++){
		if (res->type == type)
			return res->required_header;
	}
	return NULL;
}

__fly_static int __fly_required_header(fly_hdr_ci *header, char **required_header)
{
	#define __FLY_FOUND			0
	#define __FLY_NOT_FOUND		-1
	if (required_header == NULL)
		return __FLY_FOUND;

	for (char **h=required_header; *h!=NULL; h++){
#ifdef DEBUG
		assert(header != NULL);
#endif

		struct fly_bllist *__b;
		fly_hdr_c *e;
		fly_for_each_bllist(__b, &header->chain){
			e = fly_bllist_data(__b, fly_hdr_c, blelem);
			if (e->name != NULL && strcmp(e->name, *h) == 0)
				continue;
		}
		goto not_found;
	}

	goto found;
not_found:
	return __FLY_NOT_FOUND;
found:
	return __FLY_FOUND;

	#undef __FLY_FOUND
	#undef __FLY_NOT_FOUND
}

__unused __fly_static int __fly_response_required_header(fly_response_t *response)
{
	fly_stcode_t status_code = response->status_code;

	char **required_header;

	if ((required_header=__fly_stcode_required_header(status_code)) == NULL)
		return 0;

	return __fly_required_header(response->header, required_header);
}

__fly_static int __fly_status_code_from_type(fly_stcode_t type)
{
	for (fly_status_code *st=responses; st->status_code!=-1; st++){
		if (st->type == type)
			return st->status_code;
	}
	return -1;
}

fly_stcode_t fly_status_code_from_long(long __l)
{
	for (fly_status_code *st=responses; st->status_code!=-1; st++){
		if ((long) st->status_code == __l)
			return st->type;
	}
	return INVALID_STATUS_CODE;
}

const char *fly_status_code_str_from_type(fly_stcode_t type)
{
	for (fly_status_code *st=responses; st->status_code!=-1; st++){
		if (st->type == type)
			return st->status_code_str;
	}
	return NULL;
}
__fly_static int __fly_status_line(char *status_line, size_t n, fly_version_e version,fly_stcode_t stcode)
{
	char verstr[FLY_VERSION_MAXLEN];
	if (fly_version_str(verstr, version) == -1)
		return -1;
	return snprintf(
		status_line,
		n,
		"%s %d %s\r\n",
		verstr,
		__fly_status_code_from_type(stcode),
		fly_stcode_explain(stcode)
	);
}

char *fly_stcode_explain(fly_stcode_t type)
{
	for (fly_status_code *res=responses; res->status_code!=-1; res++){
		if (res->type == type)
			return res->explain;
	}
	return NULL;
}

static int fly_reponse_timeout_release_handler(fly_event_t *e);
void fly_response_timeout_end_setting(fly_event_t *e, fly_response_t *res)
{
	FLY_EVENT_END_HANDLER(e, fly_reponse_timeout_release_handler, res);
	FLY_EVENT_EXPIRED_HANDLER(e, fly_reponse_timeout_release_handler, res);
}

static int fly_reponse_timeout_release_handler(struct fly_event *e)
{
	fly_request_t *req;
	fly_connect_t *con;
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	req = res->request;
	con = req->connect;

	e->flag = FLY_CLOSE_EV;
	if (req != NULL)
		fly_request_release(req);
	fly_response_release(res);
	if (fly_connect_release(con) == -1)
		return -1;

	return 0;
}

__fly_static int __fly_response_release_handler(fly_event_t *e)
{
	fly_request_t *req;
	fly_connect_t *con;
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	req = res->request;
	con = req->connect;

	if (req != NULL)
		fly_request_release(req);
	fly_response_release(res);
	if (fly_connect_release(con) == -1)
		return -1;

	e->flag = FLY_CLOSE_EV;
	return 0;
}

__fly_static int __fly_response_reuse_handler(fly_event_t *e)
{
	fly_response_t *res;
	fly_request_t *req;
	fly_connect_t *con;
	fly_context_t *ctx;

	res = (fly_response_t *) e->event_data;
	req = res->request;
	con = req->connect;
	ctx = fly_context_from_event(e);

	fly_request_release(req);
	fly_response_release(res);
	req = fly_request_init(con);
	if (fly_unlikely_null(req))
		return -1;

	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = 0;
	e->eflag = 0;
	fly_sec(&e->timeout, ctx->request_timeout);
	FLY_EVENT_HANDLER(e, fly_request_event_handler);
	e->event_data = (void *) req;
	e->available = false;
	e->expired = false;
	e->event_fase = EFLY_REQUEST_FASE_INIT;
	e->event_state = EFLY_REQUEST_STATE_INIT;
	FLY_EVENT_EXPIRED_END_HANDLER(e, fly_request_timeout_handler, req);
	fly_event_socket(e);

	return fly_event_register(e);
}

__fly_static char *fly_log_request_line_modify(fly_reqlinec_t *__r)
{
	char *ptr = __r;
	while(*ptr && *ptr!='\r' && *ptr!='\n')
		ptr++;

	*ptr = '\0';
	return __r;
}

char *fly_log_request_line_hv2(fly_response_t *res)
{
#define FLY_LOG_REQUEST_LINE_HV2_SPACE_SIZE			1
#define FLY_LOG_REQUEST_LINE_HV2_STREND_SIZE		1
	size_t size=0;
	char   *request_line;

	size += strlen(res->request->request_line->method->name);
	size += FLY_LOG_REQUEST_LINE_HV2_SPACE_SIZE;
	size += (size_t) res->request->request_line->uri.len;
	size += FLY_LOG_REQUEST_LINE_HV2_SPACE_SIZE;
	size += strlen(res->request->request_line->version->full);
	size += FLY_LOG_REQUEST_LINE_HV2_STREND_SIZE;

	request_line = fly_pballoc(res->pool, size);
	if (fly_unlikely_null(request_line))
		return NULL;

	snprintf(request_line, size, "%s %s %s",
			res->request->request_line->method->name,
			res->request->request_line->uri.ptr,
			res->request->request_line->version->full
	);
	return request_line;
}

__fly_static int __fly_response_logcontent(fly_response_t *response, fly_event_t *e, fly_logcont_t *lc)
{
#define __FLY_RESPONSE_LOGCONTENT_SUCCESS			1
#define __FLY_RESPONSE_LOGCONTENT_ERROR				-1
#define __FLY_RESPONSE_LOGCONTENT_OVERFLOW			0
#define __FLY_RESPONSE_LOGCONTENT_REQUEST_LINE		\
	(response->request->request_line->version->type == V2 ? \
	 /* HTTP2 */											\
	 fly_log_request_line_hv2(response)						\
	 : (	\
	 /* HTTP1.1 */											\
	(response->request->request_line->request_line!=NULL ? fly_log_request_line_modify(response->request->request_line->request_line) : FLY_RESPONSE_NONSTRING)))
	/*
	 *	Peer IP: Port ---> My IP: Port, Request Line, Response Code
	 *
	 */
	int res;
	res = snprintf(
		(char *) lc->content,
		(size_t) lc->contlen,
		"%s:%s (%s) --> %s:%s (%d %s)\n",
		/* peer hostname */
		response->request->connect->hostname,
		/* peer service */
		response->request->connect->servname,
		/* request_line */
		response->request->request_line != NULL ? __FLY_RESPONSE_LOGCONTENT_REQUEST_LINE : FLY_RESPONSE_NONSTRING,
		/* hostname */
		e->manager->ctx->listen_sock->hostname,
		/* service */
		e->manager->ctx->listen_sock->servname,
		/* status code */
		__fly_status_code_from_type(response->status_code),
		/* explain of status code*/
		fly_stcode_explain(response->status_code)
	);
	if (res >= (int) fly_maxlog_length(lc->contlen)){
		memcpy(fly_maxlog_suffix_point(lc->content,lc->contlen), FLY_LOGMAX_SUFFIX, strlen(FLY_LOGMAX_SUFFIX));
		return __FLY_RESPONSE_LOGCONTENT_OVERFLOW;
	}else if (res < 0)
		return __FLY_RESPONSE_LOGCONTENT_ERROR;
	lc->contlen = res;
	return __FLY_RESPONSE_LOGCONTENT_SUCCESS;
}

int fly_response_log(fly_response_t *res, fly_event_t *e)
{
	fly_logcont_t *log_content;

	fly_event_t *le;
	le = fly_event_init(e->manager);
	if (fly_unlikely_null(le)){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ERR,
			"log event init error. %s",
			strerror(errno)
		);
		fly_event_error_add(e, __err);
		return -1;
	}

	log_content = fly_logcont_init(fly_log_from_event(e), FLY_LOG_ACCESS);
	if (log_content == NULL){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ERR,
			"creat log content error. %s (%s: %s)",
			strerror(errno),
			__FILE__,
			__LINE__
		);
		fly_event_error_add(e, __err);
		return -1;
	}
	if (fly_logcont_setting(log_content, FLY_RESPONSE_LOG_LENGTH) == -1){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ERR,
			"setting log content error. %s (%s: %s)",
			strerror(errno),
			__FILE__,
			__LINE__
		);
		fly_event_error_add(e, __err);
		return -1;
	}

	switch(__fly_response_logcontent(res, e, log_content)){
	case __FLY_RESPONSE_LOGCONTENT_SUCCESS:
	case __FLY_RESPONSE_LOGCONTENT_OVERFLOW:
		break;
	case __FLY_RESPONSE_LOGCONTENT_ERROR:
		;
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_CRIT,
			"log content error. %s (%s: %s)",
			strerror(errno),
			__FILE__,
			__LINE__
		);
		fly_event_error_add(e, __err);
		break;
	default:
		FLY_NOT_COME_HERE
	}
	if (fly_log_now(&log_content->when) == -1){
		struct fly_err *__err;
		__err = fly_event_err_init(
			e, errno, FLY_ERR_ERR,
			"qetting log time error. %s (%s: %s)",
			strerror(errno),
			__FILE__,
			__LINE__
		);
		fly_event_error_add(e, __err);
		return -1;
	}

	FLY_EVENT_HANDLER(le, fly_log_event_handler);
	le->read_or_write = FLY_WRITE;
	le->event_fase = EFLY_LOG_FASE_INIT;
	le->event_state = EFLY_LOG_STATE_WAIT;
	le->event_data = (void *) log_content;
	le->flag = 0;
	le->tflag = 0;
	le->eflag = 0;
	le->expired = false;
	fly_time_zero(le->timeout);
	/* regular file event */
	fly_event_regular(le);

	return fly_event_register(le);
}

__fly_static int fly_after_response(fly_event_t *e, fly_response_t *response)
{
	e->event_data = (void *) response;
	switch (fly_connection(response->header)){
	case FLY_CONNECTION_CLOSE:
		return __fly_response_release_handler(e);

	case FLY_CONNECTION_KEEP_ALIVE:
		return __fly_response_reuse_handler(e);

	default:
		FLY_NOT_COME_HERE
	}
}

int fly_response_set_send_ptr(fly_response_t *res);
int fly_response_send_blocking(fly_event_t *e, fly_response_t *res, int read_or_write)
{
	e->event_data = (void *) res;
	e->read_or_write = read_or_write;
	e->tflag = FLY_INHERIT;
	e->flag = FLY_MODIFY;
	e->available = false;
	FLY_EVENT_HANDLER(e, fly_response_event);
	return fly_event_register(e);
}

int fly_response_send(fly_event_t *e, fly_response_t *res);
int fly_response_event(fly_event_t *e)
{
	fly_response_t *res;
	fly_request_t *req;
	fly_rcbs_t *rcbs=NULL;

	res = (fly_response_t *) e->event_data;

	if (res->send_ptr)
		goto end_of_encoding;

	/* if there is default content, add content-length header */
	if (res->body == NULL && res->pf == NULL){
		rcbs = fly_default_content_by_stcode_from_event(e, res->status_code);
		if (rcbs){
			if (fly_add_content_length_from_fd(res->header, rcbs->fd, false) == -1){
				struct fly_err *__err;
				__err = fly_event_err_init(
					e, errno, FLY_ERR_ERR,
					"content length error from fd. (%s: %s)",
					__FILE__,
					__LINE__
				);
				fly_event_error_add(e, __err);
				return -1;
			}
			if (fly_add_content_type(res->header, rcbs->mime, false) == -1){
				struct fly_err *__err;
				__err = fly_event_err_init(
					e, errno, FLY_ERR_ERR,
					"content type error from rcbs. (%s: %s)",
					__FILE__,
					__LINE__
				);
				fly_event_error_add(e, __err);
				return -1;
			}
		}
	}

	if (res->body){
		res->response_len = res->body->body_len;
		res->type = FLY_RESPONSE_TYPE_BODY;
	}else if (res->pf){
		if (res->pf->encoded){
			res->type = FLY_RESPONSE_TYPE_ENCODED;
			res->de = res->pf->de;
			res->response_len = res->de->contlen;
			res->original_response_len = res->pf->fs.st_size;
			res->encoded = true;
			res->encoding_type = res->de->etype;
		}else{
			res->response_len = res->count;
			res->original_response_len = res->response_len;
			res->type = FLY_RESPONSE_TYPE_PATH_FILE;
		}
	}else if (res->rcbs){
		if (res->rcbs->encoded){
			res->type = FLY_RESPONSE_TYPE_ENCODED;
			res->de = res->rcbs->de;
			res->response_len = res->de->contlen;
			res->original_response_len = res->rcbs->fs.st_size;
			res->encoded = true;
			res->encoding_type = res->de->etype;
		}else{
			res->response_len = rcbs->fs.st_size;
			res->original_response_len = res->response_len;
			res->type = FLY_RESPONSE_TYPE_DEFAULT;
		}
	}else{
		res->response_len = 0;
		res->type = FLY_RESPONSE_TYPE_NOCONTENT;
	}

	/* encoding matching test */
	if (res->encoded \
			&& !fly_encoding_matching(res->request->encoding, res->encoding_type)){
		res->encoded = false;
		res->response_len = res->original_response_len;
	}

	if (res->encoded || fly_over_encoding_threshold_from_response(res)){
		if (!res->encoded)
			res->encoding_type = fly_decided_encoding_type(res->request->encoding);
		fly_add_content_encoding(res->header, res->encoding_type, false);
	}else
		res->encoding_type = NULL;


	if (!res->encoded && fly_encode_do(res)){
		res->type = FLY_RESPONSE_TYPE_ENCODED;
		if (res->encoding_type->type == fly_identity)
			goto end_of_encoding;

		fly_de_t *__de;

		__de = fly_de_init(res->pool);
		if (res->pf){
			__de->decbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_DECBUF_INIT_LEN, FLY_RESPONSE_DECBUF_CHAIN_MAX, FLY_RESPONSE_DECBUF_PER_LEN);
			__de->type = FLY_DE_FROM_PATH;
			__de->fd = res->pf->fd;
			__de->offset = res->offset;
			__de->count = res->pf->fs.st_size;
		}else if (res->rcbs){
			__de->decbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_DECBUF_INIT_LEN, FLY_RESPONSE_DECBUF_CHAIN_MAX, FLY_RESPONSE_DECBUF_PER_LEN);
			__de->type = FLY_DE_FROM_PATH;
			__de->fd = res->rcbs->fd;
			__de->offset = 0;
			__de->count = res->rcbs->fs.st_size;
		}else if (res->body){
			__de->type = FLY_DE_ENCODE;
			__de->already_ptr = res->body->body;
			__de->already_len = res->body->body_len;
			__de->target_already_alloc = true;
		}else
			FLY_NOT_COME_HERE

		size_t __max;
		__max = fly_response_content_max_length();
		__de->encbuf = fly_buffer_init(__de->pool, FLY_RESPONSE_ENCBUF_INIT_LEN, FLY_RESPONSE_ENCBUF_CHAIN_MAX(__max), FLY_RESPONSE_ENCBUF_PER_LEN);
#ifdef DEBUG
		assert(__max < (size_t) (__de->encbuf->per_len*__de->encbuf->chain_max));
#endif
		__de->event = e;
		__de->response = res;
		__de->c_sockfd = e->fd;
		__de->etype = res->encoding_type;
		__de->bfs = 0;
		__de->end = false;
		res->de = __de;

		if (fly_unlikely_null(__de->decbuf) || \
				fly_unlikely_null(__de->encbuf)){
			struct fly_err *__err;
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"response de buffer alloc error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			return -1;
		}
		if (res->encoding_type->encode(__de) == -1){
			struct fly_err *__err;
			__err = fly_event_err_init(
				e, errno, FLY_ERR_ERR,
				"response encoding error. %s",
				strerror(errno)
			);
			fly_event_error_add(e, __err);
			return -1;
		}

		res->encoded = true;
		res->response_len = __de->contlen;
		res->type = FLY_RESPONSE_TYPE_ENCODED;
	}
	if (res->de && res->de->overflow)
		goto response_413;

	if (res->de)
		fly_add_content_length(res->header, res->de->contlen, false);
	else
		fly_add_content_length(res->header, res->response_len, false);

end_of_encoding:
	if (fly_response_set_send_ptr(res) == -1)
		goto response_500;

	switch(fly_response_send(e, res)){
	case FLY_RESPONSE_SUCCESS:
		break;
	case FLY_RESPONSE_READ_BLOCKING:
		return fly_response_send_blocking(e, res, FLY_READ);
	case FLY_RESPONSE_WRITE_BLOCKING:
		return fly_response_send_blocking(e, res, FLY_WRITE);
	case FLY_RESPONSE_ERROR:
		return -1;
	default:
		FLY_NOT_COME_HERE
	}

	if (fly_response_log(res, e) == -1)
		return -1;
	return fly_after_response(e, res);

response_413:

	req = res->request;
	fly_response_release(res);
	res = fly_413_response(req);
	e->event_data = (void *) res;
	return fly_response_event(e);

response_500:

	req = res->request;
	fly_response_release(res);
	res = fly_500_response(req);
	e->event_data = (void *) res;
	return fly_response_event(e);
}

void fly_response_release(fly_response_t *response)
{
	if (response == NULL)
		return;

	if (response->header != NULL)
		fly_header_release(response->header);
	if (response->body != NULL)
		fly_body_release(response->body);
	if (response->de != NULL && \
			((!response->pf || response->pf->de!=response->de) && \
			(!response->rcbs || response->rcbs->de!=response->de)))
		fly_de_release(response->de);

	fly_delete_pool(response->pool);
}

__noreturn void fly_response_init_errorp(fly_pool_t *pool)
{
	struct fly_err *__err;
	__err = fly_err_init(
		pool, errno, FLY_ERR_ERR,
		"response init error. (%s: %s)",
		__FILE__,
		__LINE__
	);
	fly_error_error(__err);
}
__noreturn void fly_response_init_error(fly_request_t *req)
{
	fly_response_init_errorp(req->connect->pool);
}

fly_response_t *fly_304_response(fly_request_t *req, struct fly_mount_parts_file *pf)
{
	fly_response_t *res;

	res = fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;

	fly_response_http_version_from_request(res, req);
	res->status_code = _304;
	res->request = req;
	res->encoded = false;
	res->body = NULL;

	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	fly_add_content_etag(res->header, pf, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_304_event(fly_event_t *e)
{
	struct fly_response_content *rc;
	fly_request_t *req;

	rc = (struct fly_response_content *) e->event_data;
	req = rc->request;

	fly_response_t *res;
	res = fly_304_response(req, rc->pf);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_400_response(fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _400;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_400_event_norequest(fly_event_t *e, fly_connect_t *conn)
{
	fly_response_t *res;
	fly_context_t *ctx;
	fly_request_t *req;

	ctx = e->manager->ctx;
	res= fly_response_init(ctx);
	if (fly_unlikely_null(res))
		fly_response_init_errorp(conn->pool);

	req = fly_request_init(conn);
	if (fly_unlikely_null(req)){
		struct fly_err *__err;
		__err = fly_err_init(
			conn->pool, errno, FLY_ERR_ERR,
			"request init error in 400 response. (%s: %s)",
			__FILE__,
			__LINE__
		);
		fly_error_error(__err);
	}
	req->request_line = fly_pballoc(req->pool, sizeof(fly_reqline_t));
	memset(req->request_line, '\0', sizeof(fly_reqline_t));
	req->request_line->version = fly_match_version_from_type(V1_1);

	res->request = req;
	res->header = fly_header_init(ctx);
	res->version = V1_1;
	res->status_code = _400;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_server(res->header, false);
	fly_add_date(res->header, false);
	fly_add_connection(res->header, CLOSE);

	e->fd = conn->c_sockfd;
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	return fly_event_register(e);
}

int fly_400_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_400_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_404_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _404;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_404_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_404_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_405_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _405;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_405_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_405_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}


fly_response_t *fly_414_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _414;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_414_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_414_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_413_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (req->request_line != NULL && is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _413;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, CLOSE);

	return res;
}

int fly_413_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_413_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_415_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _415;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_415_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_415_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_500_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init(req->ctx);
	if (fly_unlikely_null(res))
		fly_response_init_error(req);

	res->header = fly_header_init(req->ctx);
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	fly_response_http_version_from_request(res, req);
	res->status_code = _500;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_server(res->header, is_fly_request_http_v2(req));
	fly_add_date(res->header, is_fly_request_http_v2(req));
	if (!is_fly_request_http_v2(req))
		fly_add_connection(res->header, KEEP_ALIVE);

	return res;
}

int fly_500_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_500_response(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, fly_response_event);
	e->available = false;
	e->event_data = (void *) res;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, res);
	return fly_event_register(e);
}

fly_response_t *fly_respf(fly_request_t *req, struct fly_mount_parts_file *pf)
{
	fly_response_t *response;
	bool hv2 = is_fly_request_http_v2(req);

	response = fly_response_init(req->ctx);
	if (fly_unlikely_null(response))
		fly_response_init_error(req);

	if (pf->overflow)
		response = fly_413_response(req);
	else{
		response->request = req;
		response->status_code = _200;
		response->version = !hv2 ? V1_1 : V2;
		response->header = fly_header_init(req->ctx);
		if (hv2)
			response->header->state = req->stream->state;
		response->encoding_type = fly_encoding_from_type(pf->encode_type);
		response->pf = pf;
		response->offset = 0;
		response->count = pf->fs.st_size;
		response->byte_from_start = 0;

		fly_add_content_length_from_stat(response->header, &pf->fs, hv2);
		fly_add_content_etag(response->header, pf, hv2);
		fly_add_date(response->header, hv2);
		fly_add_last_modified(response->header, pf, hv2);
		fly_add_content_type(response->header, pf->mime_type, hv2);
		if (!hv2)
			fly_add_connection(response->header, KEEP_ALIVE);
	}

	return response;
}

void __fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf, int (*handler)(fly_event_t *e))
{
	fly_response_t *response;

	response = fly_respf(req, pf);
	if (fly_unlikely_null(response))
		fly_response_init_error(req);
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, handler);
	e->available = false;
	e->event_data = (void *) response;
	fly_event_socket(e);
	fly_response_timeout_end_setting(e, response);
}

int fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf)
{
	__fly_response_from_pf(e, req, pf, fly_response_event);
	return fly_event_register(e);
}

int fly_response_content_max_length(void)
{
	return fly_config_value_int(FLY_MAX_RESPONSE_CONTENT_LENGTH);
}

int fly_response_set_send_ptr(fly_response_t *response)
{
	enum{
		STATUS_LINE,
		HEADER_LINE,
		HEADER_END,
		BODY,
	} state;
	char __status_line[FLY_STATUS_LINE_MAX];
	size_t status_len;
	size_t total;

	total = 0;
	state = STATUS_LINE;
	while(true){
		switch(state){
		case STATUS_LINE:
			{
				int result;
				result = __fly_status_line(__status_line, FLY_STATUS_LINE_MAX, response->version, response->status_code);
				if (result == -1 || result == FLY_STATUS_LINE_MAX)
					return -1;
				total += result;
				status_len = result;

				if (response->header && response->header->chain_count)
					state = HEADER_LINE;
				else
					state = HEADER_END;
				continue;
			}
		case HEADER_LINE:
			{
				struct fly_bllist *__b;
				fly_for_each_bllist(__b, &response->header->chain){
					fly_hdr_c *__c;
					__c = fly_bllist_data(__b, fly_hdr_c, blelem);
					/* name: value\r\n*/
					total += __c->name_len;
					total += strlen(": ");
					total += __c->value_len;
					total += FLY_CRLF_LENGTH;
				}
				state = HEADER_END;
				continue;
			}
		case HEADER_END:
			{
				total += FLY_CRLF_LENGTH;
				state = BODY;
				continue;
			}
		case BODY:
			{
				total += response->response_len;
				break;
			}
		}
		break;
	}

	response->send_len = total;
	state = STATUS_LINE;
	response->send_ptr = fly_pballoc(response->pool, sizeof(uint8_t)*total);
	if (fly_unlikely_null(response->send_ptr))
		return -1;
	int read_fd;

	uint8_t *ptr = response->send_ptr;
	while(true){
		switch(state){
		case STATUS_LINE:
			{
				memcpy(ptr, __status_line, status_len);
				ptr += status_len;
				if (response->header && response->header->chain_count)
					state = HEADER_LINE;
				else
					state = HEADER_END;
				continue;
			}
		case HEADER_LINE:
			{
				struct fly_bllist *__b;
				fly_for_each_bllist(__b, &response->header->chain){
					fly_hdr_c *__c;
					__c = fly_bllist_data(__b, fly_hdr_c, blelem);
					/* name: value\r\n*/
					memcpy(ptr, __c->name, __c->name_len);
					ptr += __c->name_len;
					memcpy(ptr, ": ", 2);
					ptr += strlen(": ");
					memcpy(ptr, __c->value, __c->value_len);
					ptr += __c->value_len;
					memcpy(ptr, FLY_CRLF, FLY_CRLF_LENGTH);
					ptr += FLY_CRLF_LENGTH;
				}
				state = HEADER_END;
				continue;
			}
		case HEADER_END:
			{
				memcpy(ptr, FLY_CRLF, FLY_CRLF_LENGTH);
				ptr += FLY_CRLF_LENGTH;
				state = BODY;
				continue;
			}
		case BODY:
			{
				switch(response->type){
				case FLY_RESPONSE_TYPE_ENCODED:
					fly_buffer_memncpy_all((char *) ptr, response->de->encbuf, response->send_ptr+total-(void *) ptr);
					break;
				case FLY_RESPONSE_TYPE_BODY:
					memcpy(ptr, response->body->body, response->body->body_len);
					ptr += response->body->body_len;
					break;
				case FLY_RESPONSE_TYPE_PATH_FILE:
					read_fd = response->pf->fd;
					goto copy_file;
				case FLY_RESPONSE_TYPE_DEFAULT:
					read_fd = response->pf->fd;
					goto copy_file;
copy_file:
					{
						ssize_t numread;

						if (lseek(read_fd, response->offset, SEEK_SET) == -1){

							struct fly_err *__err;
							__err = fly_err_init(
								response->pool, errno, FLY_ERR_ERR,
								"lseek error in set send ptr of response. (%s: %s)",
								__FILE__, __LINE__
							);
							fly_error_error(__err);
							FLY_NOT_COME_HERE
						}

retry_read:
						while((numread = read(read_fd, ptr, response->count)) > 0){
							ptr += numread;
							/* overflow */
							if ((char *) ptr > (char *) (((char *) response->send_ptr)+total))
								return -1;
						}

						if (numread == -1){
							if (errno == EINTR)
								goto retry_read;
							else{
								struct fly_err *__err;
								__err = fly_err_init(
									response->pool, errno, FLY_ERR_ERR,
									"read error in set send ptr of response. (%s: %s)",
									__FILE__, __LINE__
								);
								fly_error_error(__err);
							}
							FLY_NOT_COME_HERE
						}

						/* EOF */
						break;
					}
					break;
				case FLY_RESPONSE_TYPE_NOCONTENT:
					break;
				default:
					FLY_NOT_COME_HERE
				}
				break;
			}
		}
		break;
	}
	return 0;
}

int fly_response_send(fly_event_t *e, fly_response_t *res)
{
	size_t total = 0;
	char *ptr;

	if (res->byte_from_start > 0)
		total = res->byte_from_start;

	ptr = res->send_ptr + total;
	while(total < res->send_len){
		ssize_t sendnum;

		if (FLY_CONNECT_ON_SSL(res->request->connect)){
			SSL *ssl=res->request->connect->ssl;
			sendnum = SSL_write(ssl, ptr, res->send_len-total);
			switch(SSL_get_error(ssl, sendnum)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_WANT_READ:
				return FLY_RESPONSE_READ_BLOCKING;
			case SSL_ERROR_WANT_WRITE:
				return FLY_RESPONSE_WRITE_BLOCKING;
			case SSL_ERROR_SYSCALL:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_SSL:
				return FLY_RESPONSE_ERROR;
			default:
				/* unknown error */
				return FLY_RESPONSE_ERROR;
			}
		}else{
			sendnum = send(e->fd, ptr, res->send_len-total, 0);
			if (FLY_BLOCKING(sendnum)){
				return FLY_RESPONSE_WRITE_BLOCKING;
			}else if (sendnum == -1)
				return FLY_RESPONSE_ERROR;
		}
		total += sendnum;
	}
	return FLY_RESPONSE_SUCCESS;
}
