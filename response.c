#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "response.h"
#include "request.h"
#include "header.h"
#include "alloc.h"
#include "mount.h"
#include "math.h"
#include "v2.h"

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
int __fly_response_log(fly_response_t *res, fly_event_t *e);
__fly_static int __fly_response_logcontent(fly_response_t *response, fly_event_t *e, fly_logcont_t *lc);
__fly_static int __fly_send_until_header(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_send_until_header_blocking(fly_event_t *e, fly_response_t *response, int flag);
__fly_static int __fly_send_until_header_blocking_handler(fly_event_t *e);
__fly_static int __fly_send_body_blocking_handler(fly_event_t *e);
__fly_static int __fly_send_body_blocking(fly_event_t *e, fly_response_t *response, int read_or_write);
__fly_static int __fly_send_body(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_after_response(fly_event_t *e, fly_response_t *response);

fly_response_t *fly_response_init(void)
{
	fly_response_t *response;
	fly_pool_t *pool;
	pool = fly_create_pool(FLY_RESPONSE_POOL_PAGE);
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
	response->encoding = NULL;
	response->de = NULL;
	response->blocking = false;
	response->encoded = false;
	response->rcbs = NULL;
	return response;
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
		if (header == NULL)
			goto not_found;

		for (fly_hdr_c *e=header->entry; e!=NULL; e=e->next){
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

const char *fly_status_code_str_from_type(fly_stcode_t type)
{
	for (fly_status_code *st=responses; st->status_code!=-1; st++){
		if (st->type == type)
			return st->status_code_str;
	}
	return NULL;
}
__fly_static int __fly_status_line(char *status_line, fly_version_e version,fly_stcode_t stcode)
{
	char verstr[FLY_VERSION_MAXLEN];
	if (fly_version_str(verstr, version) == -1)
		return -1;
	return sprintf(
		status_line,
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

__fly_static int __fly_response_release_handler(fly_event_t *e)
{
	fly_request_t *req;
	fly_connect_t *con;
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	req = res->request;
	con = req->connect;

	if (fly_request_release(req) == -1)
		return -1;
	fly_response_release(res);
	if (fly_connect_release(con) == -1)
		return -1;

	e->flag = FLY_CLOSE_EV;
	if (fly_event_unregister(e) == -1)
		return -1;

	return 0;
}

__fly_static int __fly_response_reuse_handler(fly_event_t *e)
{
	fly_response_t *res;
	fly_request_t *req;
	fly_connect_t *con;

	res = (fly_response_t *) e->event_data;
	req = res->request;
	con = req->connect;

	if (fly_request_release(req) == -1)
		return -1;
	fly_response_release(res);
	req = fly_request_init(con);
	if (req == NULL)
		return -1;

	e->read_or_write = FLY_READ;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, fly_request_event_handler);
	e->event_data = (void *) req;
	e->available = false;
	e->event_fase = EFLY_REQUEST_FASE_INIT;
	e->event_state = EFLY_REQUEST_STATE_INIT;
	fly_event_socket(e);

	if (fly_event_register(e) == -1)
		return -1;

	return 0;
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
	(response->request->request_line->request_line ? fly_log_request_line_modify(response->request->request_line->request_line) : FLY_RESPONSE_NONSTRING)))
	/* TODO: configuable log design. */
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
	}
	lc->contlen = res;
	return __FLY_RESPONSE_LOGCONTENT_SUCCESS;
}

int __fly_response_log(fly_response_t *res, fly_event_t *e)
{
	fly_logcont_t *log_content;

	fly_event_t *le;
	le = fly_event_init(e->manager);
	if (le == NULL)
		return -1;

	log_content = fly_logcont_init(fly_log_from_event(e), FLY_LOG_ACCESS);
	if (log_content == NULL)
		return -1;
	if (fly_logcont_setting(log_content, FLY_RESPONSE_LOG_LENGTH) == -1)
		return -1;

	if (__fly_response_logcontent(res, e, log_content) == -1)
		return -1;
	if (fly_log_now(&log_content->when) == -1)
		return -1;

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


__fly_static int __fly_send_until_header_blocking_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	return __fly_send_until_header(e, res);
}

__fly_static int __fly_send_until_header_blocking(fly_event_t *e, fly_response_t *response, int read_or_write)
{
	e->event_data = (void *) response;
	e->read_or_write = read_or_write;
	e->eflag = 0;
	e->tflag = FLY_INHERIT;
	e->flag = FLY_NODELETE;
	e->available = false;
	FLY_EVENT_HANDLER(e, __fly_send_until_header_blocking_handler);
	return fly_event_register(e);
}

__fly_static int __fly_send_until_header(fly_event_t *e, fly_response_t *response)
{
	enum{
		STATUS_LINE,
		HEADER_LINE,
		HEADER_END,
	} state;
	int c_sockfd;
	void **send_ptr;
	int *byte_from_start;

	/* timeout handle */
	if (e->expired){
		e->event_data = (fly_response_t *) response;
		__fly_response_release_handler(e);
	}

	state = STATUS_LINE;
	switch (response->fase){
	case FLY_RESPONSE_READY:
		state = STATUS_LINE;
		break;
	case FLY_RESPONSE_STATUS_LINE:
		state = STATUS_LINE;
		break;
	case FLY_RESPONSE_HEADER:
		state = HEADER_LINE;
		break;
	case FLY_RESPONSE_CRLF:
		state = HEADER_END;
		break;
	default:
		return FLY_RESPONSE_ERROR;
	}

	c_sockfd = response->request->connect->c_sockfd;
	send_ptr = &response->send_ptr;
	byte_from_start = &response->byte_from_start;
	while(true){
		switch(state){
		case STATUS_LINE:
			{
				int result, total=0, numsend;
				char __status_line[FLY_STATUS_LINE_MAX];
				result = __fly_status_line(__status_line, response->version, response->status_code);
				if (result == -1)	return FLY_RESPONSE_ERROR;

				response->fase = FLY_RESPONSE_STATUS_LINE;
				if (byte_from_start)
					total = *byte_from_start;
				while(result > total){
					if (FLY_CONNECT_ON_SSL(response->request->connect)){
						SSL *ssl=response->request->connect->ssl;
						numsend = SSL_write(ssl, __status_line+total, result-total);
						switch(SSL_get_error(ssl, numsend)){
						case SSL_ERROR_NONE:
							break;
						case SSL_ERROR_ZERO_RETURN:
							return FLY_RESPONSE_ERROR;
						case SSL_ERROR_WANT_READ:
							if (__fly_send_until_header_blocking(e, response, FLY_READ) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						case SSL_ERROR_WANT_WRITE:
							if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						case SSL_ERROR_SYSCALL:
							return FLY_RESPONSE_ERROR;
						case SSL_ERROR_SSL:
							return FLY_RESPONSE_ERROR;
						default:
							/* unknown error */
							return FLY_RESPONSE_ERROR;
						}
					}else{
						numsend = send(c_sockfd, __status_line+total, result-total, 0);
						if (FLY_BLOCKING(numsend)){
							if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						}else if (numsend == -1)
							return FLY_RESPONSE_ERROR;
					}

					total += numsend;
					*byte_from_start = total;
				}

				*byte_from_start = 0;
				if (response->header && response->header->chain_length)
					state = HEADER_LINE;
				else
					state = HEADER_END;
				continue;
			}
		case HEADER_LINE:
			{
				char __header_line[FLY_HEADER_LINE_MAX];
				int result, total=0, numsend;
				fly_hdr_c *start;

				response->fase = FLY_RESPONSE_HEADER;
				if (*send_ptr == NULL)
					start = response->header->entry;
				else
					start = (fly_hdr_c *) *send_ptr;

				for (fly_hdr_c *__c=start; __c; __c=__c->next){
					*send_ptr = __c;
					total = 0;
					result = snprintf(__header_line, FLY_HEADER_LINE_MAX, "%s: %s\r\n", __c->name, __c->value!=NULL ? __c->value : "");
					if (result < 0 || result >= FLY_HEADER_LINE_MAX)
						continue;

					if (*byte_from_start != 0)
						total = *byte_from_start;

					while(result > total){
						if (FLY_CONNECT_ON_SSL(response->request->connect)){
							SSL *ssl=response->request->connect->ssl;
							numsend = SSL_write(ssl, __header_line+total, result-total);
							switch(SSL_get_error(ssl, numsend)){
							case SSL_ERROR_NONE:
								break;
							case SSL_ERROR_ZERO_RETURN:
								return FLY_RESPONSE_ERROR;
							case SSL_ERROR_WANT_READ:
								if (__fly_send_until_header_blocking(e, response, FLY_READ) == -1)
									return FLY_RESPONSE_ERROR;
								return FLY_RESPONSE_BLOCKING;
							case SSL_ERROR_WANT_WRITE:
								if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
									return FLY_RESPONSE_ERROR;
								return FLY_RESPONSE_BLOCKING;
							case SSL_ERROR_SYSCALL:
								return FLY_RESPONSE_ERROR;
							case SSL_ERROR_SSL:
								return FLY_RESPONSE_ERROR;
							default:
								/* unknown error */
								return FLY_RESPONSE_ERROR;
							}
						}else{
							numsend = send(c_sockfd, __header_line+total, result-total, 0);
							if (FLY_BLOCKING(numsend)){
								if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
									return FLY_RESPONSE_ERROR;
								return FLY_RESPONSE_BLOCKING;
							}else if (numsend == -1)
								return FLY_RESPONSE_ERROR;
						}

						total += numsend;
						*byte_from_start = total;
					}
					*byte_from_start = 0;
				}

				*byte_from_start = 0;
				state = HEADER_END;
				continue;
			}
		case HEADER_END:
			{
				int numsend, total;
				total = 0;
				if (*byte_from_start)
					total = *byte_from_start;
				while((int) FLY_CRLF_LENGTH > total){
					if (FLY_CONNECT_ON_SSL(response->request->connect)){
						SSL *ssl=response->request->connect->ssl;
						numsend = SSL_write(ssl, FLY_CRLF+total, FLY_CRLF_LENGTH-total);
						switch(SSL_get_error(ssl, numsend)){
						case SSL_ERROR_NONE:
							break;
						case SSL_ERROR_ZERO_RETURN:
							return FLY_RESPONSE_ERROR;
						case SSL_ERROR_WANT_READ:
							if (__fly_send_until_header_blocking(e, response, FLY_READ) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						case SSL_ERROR_WANT_WRITE:
							if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						case SSL_ERROR_SYSCALL:
							return FLY_RESPONSE_ERROR;
						case SSL_ERROR_SSL:
							return FLY_RESPONSE_ERROR;
						default:
							/* unknown error */
							return FLY_RESPONSE_ERROR;
						}
					}else{
						numsend = send(c_sockfd, FLY_CRLF+total, FLY_CRLF_LENGTH-total, 0);
						if (FLY_BLOCKING(numsend)){
							if (__fly_send_until_header_blocking(e, response, FLY_WRITE) == -1)
								return -1;
							return FLY_RESPONSE_BLOCKING;
						}else if (numsend == -1)
							return FLY_RESPONSE_ERROR;
					}
					total += numsend;
					*byte_from_start = total;
				}
				break;
			}
		}
		break;
	}

	*send_ptr = NULL;
	*byte_from_start = 0;
	response->fase = FLY_RESPONSE_RELEASE;
	return FLY_RESPONSE_SUCCESS;
}

__fly_static int __fly_send_body_blocking_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	return __fly_send_body(e, res);
}

__fly_static int __fly_send_body_blocking(fly_event_t *e, fly_response_t *response, int read_or_write)
{
	e->event_data = (void *) response;
	e->read_or_write = read_or_write;
	e->eflag = 0;
	e->tflag = FLY_INHERIT;
	e->flag = FLY_NODELETE;
	e->available = false;
	FLY_EVENT_HANDLER(e, __fly_send_body_blocking_handler);
	return fly_event_register(e);
}

__fly_static int __fly_send_body(fly_event_t *e, fly_response_t *response)
{
	fly_body_t *body;
	int *bfs;
	fly_bodyc_t *buf;
	response->fase = FLY_RESPONSE_BODY;
	bfs = &response->byte_from_start;
	buf = response->body->body;
	response->send_ptr = buf;

	body = response->body;
	if (body->body_len == 0)
		return FLY_RESPONSE_SUCCESS;

	int total = 0;
	if (*bfs)
		total = body->body_len - *bfs;
	while(total < body->body_len){
		ssize_t sendnum;

		if (FLY_CONNECT_ON_SSL(response->request->connect)){
			SSL *ssl=response->request->connect->ssl;
			sendnum = SSL_write(ssl, buf, total);
			switch(SSL_get_error(ssl, sendnum)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_WANT_READ:
				if (__fly_send_body_blocking(e, response, FLY_READ) == -1)
					return FLY_RESPONSE_ERROR;
				return FLY_RESPONSE_BLOCKING;
			case SSL_ERROR_WANT_WRITE:
				if (__fly_send_body_blocking(e, response, FLY_WRITE) == -1)
					return FLY_RESPONSE_ERROR;
				return FLY_RESPONSE_BLOCKING;
			case SSL_ERROR_SYSCALL:
				return FLY_RESPONSE_ERROR;
			case SSL_ERROR_SSL:
				return FLY_RESPONSE_ERROR;
			default:
				/* unknown error */
				return FLY_RESPONSE_ERROR;
			}
		}else{
			sendnum = send(e->fd, buf, total, 0);
			if (FLY_BLOCKING(sendnum)){
				if (__fly_send_body_blocking(e, response, FLY_WRITE) == -1)
					return FLY_RESPONSE_ERROR;
				return FLY_RESPONSE_SUCCESS;
			}else if (sendnum == -1)
				return FLY_RESPONSE_ERROR;
		}
		total += sendnum;
	}

	*bfs = 0;
	response->send_ptr = NULL;
	response->fase = FLY_RESPONSE_RELEASE;
	return FLY_RESPONSE_SUCCESS;
}


__fly_static int __fly_after_response(fly_event_t *e, fly_response_t *response)
{
	switch (fly_connection(response->header)){
	case FLY_CONNECTION_CLOSE:
		e->event_state = (void *) EFLY_REQUEST_STATE_END;
		e->event_data = response;
		e->read_or_write = FLY_WRITE|FLY_READ;
		e->flag = FLY_CLOSE_EV | FLY_MODIFY;
		FLY_EVENT_HANDLER(e, __fly_response_release_handler);
		e->available = false;
		fly_event_socket(e);

		if (fly_event_register(e) == -1)
			goto error;
		return 0;

	case FLY_CONNECTION_KEEP_ALIVE:
		e->event_state = (void *) EFLY_REQUEST_STATE_INIT;
		e->event_fase = (void  *) EFLY_REQUEST_FASE_INIT;
		e->event_data = (void *) response;
		e->read_or_write = FLY_WRITE|FLY_READ;
		e->flag = FLY_MODIFY;
		fly_sec(&e->timeout, FLY_REQUEST_TIMEOUT);
		e->tflag = 0;
		e->eflag = 0;
		FLY_EVENT_HANDLER(e, __fly_response_reuse_handler);
		e->available = false;
		e->expired = false;
		fly_event_socket(e);

		if (fly_event_register(e) == -1)
			goto error;
		return 0;

	default:
		e->event_state = (void *) EFLY_REQUEST_STATE_END;
		e->event_data = response;
		e->read_or_write = FLY_WRITE|FLY_READ;
		e->flag = FLY_CLOSE_EV | FLY_MODIFY;
		FLY_EVENT_HANDLER(e, __fly_response_release_handler);
		e->available = false;
		if (fly_event_register(e) == -1)
			goto error;
		return -1;
	}
error:
	return -1;
}

int fly_response_event(fly_event_t *e)
{
	fly_response_t *response;
	fly_rcbs_t *rcbs=NULL;

	response = (fly_response_t *) e->event_data;

	/* if there is default content, add content-length header */
	if (response->body == NULL && response->pf == NULL){
		rcbs = fly_default_content_by_stcode_from_event(e, response->status_code);
		if (rcbs){
			if (fly_add_content_length_from_fd(response->header, rcbs->fd, false) == -1)
				return -1;
			if (fly_add_content_type(response->header, rcbs->mime, false) == -1)
				return -1;
		}
	}

	if (__fly_encode_do(response)){
		fly_encoding_type_t *enctype=NULL;
		enctype = fly_decided_encoding_type(response->encoding);
		if (fly_unlikely_null(enctype))
			return -1;


		if (enctype->type == fly_identity)
			goto end_of_encoding;

		fly_de_t *__de;

		__de = fly_pballoc(response->pool, sizeof(fly_de_t));
		__de->pool = response->pool;
		__de->type = FLY_DE_ESEND_FROM_PATH;
		__de->encbuflen = 0;
		__de->decbuflen = 0;
		__de->encbuf = fly_buffer_init(__de->pool, FLY_DE_BUF_INITLEN, FLY_DE_BUF_MAXLEN, FLY_DE_BUF_PERLEN);
		__de->decbuf = fly_buffer_init(__de->pool, FLY_DE_BUF_INITLEN, FLY_DE_BUF_MAXLEN, FLY_DE_BUF_PERLEN);
		__de->fd = response->pf->fd;
		__de->offset = response->offset;
		__de->count = response->pf->fs.st_size;
		__de->event = e;
		__de->response = response;
		__de->c_sockfd = e->fd;
		__de->etype = enctype;
		__de->fase = FLY_DE_INIT;
		__de->bfs = 0;
		__de->end = false;
		response->de = __de;

		if (fly_unlikely_null(__de->decbuf) || \
				fly_unlikely_null(__de->encbuf))
			return -1;
		if (enctype->encode(__de) == -1)
			return -1;
	}
end_of_encoding:

	switch (__fly_send_until_header(e, response)){
	case FLY_RESPONSE_SUCCESS:
		break;
	case FLY_RESPONSE_BLOCKING:
		/* event register in __fly_send_until_header */
		return 0;
	case FLY_RESPONSE_ERROR:
		return -1;
	}


	if (__fly_encode_do(response)){
		switch(fly_esend_body(e, response)){
		case FLY_RESPONSE_SUCCESS:
			break;
		case FLY_RESPONSE_ERROR:
			return -1;
		case FLY_RESPONSE_BLOCKING:
			return 0;
		}
	} else if (response->body){
		switch (__fly_send_body(e, response)){
		case FLY_RESPONSE_SUCCESS:
			break;
		case FLY_RESPONSE_BLOCKING:
			/* event register in __fly_send_body */
			return 0;
		case FLY_RESPONSE_ERROR:
			return -1;
		default:
			FLY_NOT_COME_HERE
		}
	} else if (response->pf){
		int c_sockfd;
		c_sockfd = response->request->connect->c_sockfd;
		switch(fly_send_from_pf(e, c_sockfd, response->pf, &response->offset, response->count)){
		case FLY_RESPONSE_SUCCESS:
			break;
		case FLY_RESPONSE_BLOCKING:
			/* event register in __fly_send_body */
			return 0;
		case FLY_RESPONSE_ERROR:
			return -1;
		default:
			FLY_NOT_COME_HERE
		}
	} else{
		if (rcbs){
			switch(fly_send_default_content(e, rcbs)){
			case FLY_SEND_DEFAULT_CONTENT_BY_STCODE_SUCCESS:
			case FLY_SEND_DEFAULT_CONTENT_BY_STCODE_BLOCKING:
			case FLY_SEND_DEFAULT_CONTENT_BY_STCODE_NOTFOUND:
				break;
			case FLY_SEND_DEFAULT_CONTENT_BY_STCODE_ERROR:
				return -1;
			default:
				FLY_NOT_COME_HERE
			}
		}
	}

	if (__fly_response_log(response, e) == -1)
		return -1;
	return __fly_after_response(e, response);
}

void fly_response_release(fly_response_t *response)
{
	if (response == NULL)
		return;

	if (response->header != NULL)
		fly_header_release(response->header);
	if (response->body != NULL)
		fly_body_release(response->body);
	if (response->de != NULL)
		fly_de_release(response->de);

	fly_delete_pool(&response->pool);
}

fly_response_t *fly_304_response(fly_request_t *req, struct fly_mount_parts_file *pf)
{
	fly_response_t *res;

	res = fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;

	res->version = req->request_line->version->type;
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
	if (fly_unlikely_null(res))
		return -1;

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

fly_response_t *fly_400_response(fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	res->version = V1_1;
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

int fly_400_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;

	res = fly_400_response(req);
	if (fly_unlikely_null(res))
		return -1;

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

fly_response_t *fly_404_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	res->version = V1_1;
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
	if (fly_unlikely_null(res))
		return -1;

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

fly_response_t *fly_405_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	res->version = V1_1;
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
	if (fly_unlikely_null(res))
		return -1;

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


fly_response_t *fly_414_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	res->version = V1_1;
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
	if (fly_unlikely_null(res))
		return -1;

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

fly_response_t *fly_415_response(fly_request_t *req)
{
	fly_response_t *res;

	res= fly_response_init();
	if (fly_unlikely_null(res))
		return NULL;

	res->header = fly_header_init();
	if (is_fly_request_http_v2(req))
		res->header->state = req->stream->state;
	res->version = V1_1;
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
	if (fly_unlikely_null(res))
		return -1;

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

int __fly_encode_do(fly_response_t *res)
{
	return (res->encoding && res->encoding->actqty);
}

fly_response_t *fly_respf(fly_request_t *req, struct fly_mount_parts_file *pf)
{
	fly_response_t *response;
	bool hv2 = is_fly_request_http_v2(req);

	response = fly_response_init();
	if (fly_unlikely_null(response))
		return NULL;

	response->request = req;
	response->status_code = _200;
	response->version = !hv2 ? V1_1 : V2;
	response->header = fly_header_init();
	if (hv2)
		response->header->state = req->stream->state;
	response->encoding = req->encoding;
	response->pf = pf;
	response->offset = 0;
	response->count = pf->fs.st_size;
	response->byte_from_start = 0;

	fly_add_content_length_from_stat(response->header, &pf->fs, hv2);
	fly_add_content_etag(response->header, pf, hv2);
	fly_add_date(response->header, hv2);
	fly_add_last_modified(response->header, pf, hv2);
	fly_add_content_encoding(response->header, req->encoding, hv2);
	fly_add_content_type(response->header, pf->mime_type, hv2);
	if (!hv2)
		fly_add_connection(response->header, KEEP_ALIVE);

	return response;
}

int __fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf, int (*handler)(fly_event_t *e))
{
	fly_response_t *response;

	response = fly_respf(req, pf);
	if (fly_unlikely_null(response))
		return -1;
	e->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	e->read_or_write = FLY_WRITE;
	e->flag = FLY_MODIFY;
	e->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(e, handler);
	e->available = false;
	e->event_data = (void *) response;
	fly_event_socket(e);

	return 0;
}

int fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf)
{
	if (e->expired){
		e->event_data = req;
		return fly_request_timeout(e);
	}

	if (__fly_response_from_pf(e, req, pf, fly_response_event) == -1)
		return -1;

	return fly_event_register(e);
}

