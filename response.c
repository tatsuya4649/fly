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

fly_status_code responses[] = {
	/* 1xx Info */
	{100, _100, "Continue", NULL,				NULL},
	{101, _101, "Switching Protocols", NULL,			FLY_STRING_ARRAY("Upgrade", NULL)},
	/* 2xx Success */
	{200, _200, "OK", NULL, 					NULL},
	{201, _201, "Created", NULL,				NULL},
	{202, _202, "Accepted", NULL, 				NULL},
	{203, _203, "Non-Authoritative Information", NULL, NULL},
	{204, _204, "No Content", NULL, 					NULL},
	{205, _205, "Reset Content", NULL,				NULL},
	/* 3xx Redirect */
	{300, _300, "Multiple Choices", NULL, 				NULL},
	{301, _301, "Moved Permanently", NULL,			FLY_STRING_ARRAY("Location", NULL)},
	{302, _302, "Found", NULL,						FLY_STRING_ARRAY("Location", NULL)},
	{303, _303, "See Other", NULL,					FLY_STRING_ARRAY("Location", NULL)},
	{304, _304, "Not Modified",	NULL,				NULL},
	{307, _307, "Temporary Redirect", NULL,			FLY_STRING_ARRAY("Location", NULL)},
	/* 4xx Client Error */
	{400, _400, "Bad Request", NULL,					NULL},
	{401, _401, "Unauthorized",	NULL,				NULL},
	{402, _402, "Payment Required", NULL,				NULL},
	{403, _403, "Forbidden", NULL,					NULL},
	{404, _404, "Not Found", FLY_PATH_FROM_STATIC(404.html),	NULL},
	{405, _405, "Method Not Allowed", FLY_PATH_FROM_STATIC(405.html),			FLY_STRING_ARRAY("Allow", NULL)},
	{406, _406, "Not Acceptable", NULL,				NULL},
	{407, _407, "Proxy Authentication Required", NULL, NULL},
	{408, _408, "Request Timeout", NULL,				FLY_STRING_ARRAY("Connection", NULL)},
	{409, _409, "Conflict", NULL,						NULL},
	{409, _410, "Gone", NULL,							NULL},
	{410, _411, "Length Required", NULL,				NULL},
	{413, _413, "Payload Too Large", NULL,			FLY_STRING_ARRAY("Retry-After", NULL)},
	{414, _414, "URI Too Long", NULL,					NULL},
	{415, _415, "Unsupported Media Type", NULL,		NULL},
	{416, _416, "Range Not Satisfiable", NULL,		NULL},
	{417, _417, "Expectation Failed", NULL,			NULL},
	{426, _426, "Upgrade Required", NULL,				FLY_STRING_ARRAY("Upgrade", NULL)},
	/* 5xx Server Error */
	{500, _500, "Internal Server Error", NULL,		NULL},
	{501, _501, "Not Implemented", NULL,				NULL},
	{502, _502, "Bad Gateway", NULL,			NULL},
	{503, _503, "Service Unavailable", NULL,		NULL},
	{504, _504, "Gateway Timeout", NULL,				NULL},
	{505, _505, "HTTP Version Not SUpported", NULL,	NULL},
	{-1, -1, NULL, NULL, NULL}
};

__fly_static void __fly_500_error(fly_event_t *e, fly_version_e version, fly_itm_response_t *itm);
__fly_static void __fly_5xx_error(fly_event_t *e, fly_version_e version, fly_itm_response_t *itm);
__fly_static int __fly_response(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_response_release_handler(fly_event_t *e);
#define FLY_RESPONSE_LOG_ITM_FLAG		(1)
__fly_static int __fly_response_log(fly_response_t *res, fly_event_t *e);
__fly_static int __fly_response_logcontent(fly_response_t *response, fly_event_t *e, fly_logcont_t *lc);
__fly_static int __fly_send_until_header(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_send_until_header_blocking(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_send_until_header_blocking_handler(fly_event_t *e);
__fly_static int __fly_send_body_blocking_handler(fly_event_t *e);
__fly_static int __fly_send_body_blocking(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_send_body(fly_event_t *e, fly_response_t *response);
__fly_static int __fly_after_response(fly_event_t *e, fly_response_t *response);
inline static int __fly_encode_do(fly_response_t *res);

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
	return response;
}

__fly_static char *__fly_alloc_response_content(fly_pool_t *pool,char *now,int *before,int incre)
{
	char *resp;
	resp = fly_pballoc(pool, *before+incre);
	if (resp != now)
		memcpy(resp, now, *before);
	*before += incre;
	return resp;
}

__fly_static char *__fly_update_alloc_response_content(
	fly_pool_t *pool,
	char *response_content,
	int pos,
	int length,
	int total_length,
	int incre
)
{
	while ((pos+length) > total_length){
		response_content = __fly_alloc_response_content(
			pool,
			response_content,
			&total_length,
			incre
		);
	}
	return response_content;
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

__fly_static int __fly_response_required_header(fly_response_t *response)
{
	fly_stcode_t status_code = response->status_code;

	char **required_header;

	if ((required_header=__fly_stcode_required_header(status_code)) == NULL)
		return 0;

	return __fly_required_header(response->header, required_header);
}

__fly_static char *__fly_add_cr_lf(
	fly_pool_t *pool,
	char *response_content,
	int *pos,
	int total_length,
	int incre
)
{
	char *res;
	res = __fly_update_alloc_response_content(
		pool,
		response_content,
		*pos,
		FLY_CRLF_LENGTH,
		total_length,
		incre
	);
	strcpy(&response_content[*pos], "\r\n");
	*pos += FLY_CRLF_LENGTH;
	return res;
}

__fly_static int __fly_status_code_from_type(fly_stcode_t type)
{
	for (fly_status_code *st=responses; st->status_code!=-1; st++){
		if (st->type == type)
			return st->status_code;
	}
	return -1;
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

__fly_static char *__fly_response_raw(fly_response_t *res, int *send_len)
{
	char *response_content = NULL;
	int total_length = 0;
	int header_length = 0;
	int pos = 0;

	response_content = __fly_alloc_response_content(
		res->pool,
		response_content,
		&total_length,
		RESPONSE_LENGTH_PER
	);
	/* status_line */
	if (__fly_status_line(&response_content[pos], res->version, res->status_code) == -1)
		return NULL;
	pos += (int) strlen(&response_content[pos]);
	/* header */
	header_length = fly_hdrlen_from_chain(res->header);
	if (header_length == -1)
		return NULL;

	if (res->header!=NULL && header_length>0){
		__fly_update_alloc_response_content(
			res->pool,
			response_content,
			pos,
			header_length,
			total_length,
			RESPONSE_LENGTH_PER
		);
		memcpy(&response_content[pos], fly_header_from_chain(res->header), header_length);
		pos += header_length;
	}
	if (header_length != 0 && res->body->body_len > 0){
		__fly_add_cr_lf(res->pool, response_content, &pos, total_length, RESPONSE_LENGTH_PER);
	}
	/* body */
	if (res->body->body!=NULL && res->body->body_len>0){
		__fly_update_alloc_response_content(
			res->pool,
			response_content,
			pos,
			(int) res->body->body_len,
			total_length,
			RESPONSE_LENGTH_PER
		);
		memcpy(&response_content[pos], res->body->body, res->body->body_len);
		pos += res->body->body_len;
	}
	*send_len = pos;
	return response_content;
}

char *fly_stcode_explain(fly_stcode_t type)
{
	for (fly_status_code *res=responses; res->status_code!=-1; res++){
		if (res->type == type)
			return res->explain;
	}
	return NULL;
}

__fly_static int __fly_send(int c_sockfd, char *buffer, int send_len, int flag)
{
	int send_result;
	while(1){
		send_result = send(c_sockfd, buffer, send_len, flag);
		if (send_result == -1)
			return -1;
		if (send_result == send_len){
			break;
		}
		buffer += send_result;
		send_len -= send_result;
	}
	return 0;
}

__fly_static void __fly_4xx_error(fly_event_t *e, fly_version_e version, fly_itm_response_t *itm)
{
	fly_response_t *response;
	fly_stcode_t status;
	fly_hdr_ci *ci;
	char *contlen_str;
	fly_body_t *body;
	int contlen;

	status = itm->status_code;
	ci = fly_header_init();
	response = fly_response_init();

	contlen = strlen(fly_stcode_explain(status));
	contlen_str = fly_pballoc(ci->pool, fly_number_digits(contlen)+1);
	sprintf(contlen_str, "%d", contlen);
	if (fly_header_add(ci, fly_header_name_length("Content-Length"), fly_header_value_length(contlen_str)) == -1)
		goto error;
	if (fly_header_add(ci, fly_header_name_length("Connection"), fly_header_value_length("close")) == -1)
		goto error;

	body = fly_body_init();
	body->body = fly_stcode_explain(status);
	body->body_len = strlen(body->body);

	response->status_code = status;
	response->version = version;
	response->header = ci;
	response->body = body;
	response->request = itm->req;
	if (__fly_response_log(response, e) == -1)
		goto error;
    if (__fly_response(e, response) == -1)
		goto error;
	goto end;

error:
	__fly_500_error(e, V1_1, itm);
end:
	fly_header_release(ci);
	fly_body_release(body);
	fly_response_release(response);
	return;
}

__fly_static void __fly_500_error(fly_event_t *e, fly_version_e version, fly_itm_response_t *itm)
{
	itm->status_code = _500;
	__fly_5xx_error(e, version, itm);
}

__fly_static void __fly_5xx_error(fly_event_t *e, fly_version_e version, fly_itm_response_t *itm)
{
	fly_response_t *response;
	fly_stcode_t status_code;
	fly_body_t *body;

	status_code = itm->status_code;
	response = fly_response_init();
	if (response == NULL)
		return;

	response->status_code = status_code;
	response->version = version;
	response->header = NULL;

	body = fly_body_init();
	body->body = fly_stcode_explain(_500);
	body->body_len = strlen(body->body);
	response->body = body;
	response->request = itm->req;

	__fly_response_log(response, e);
	__fly_response(e, response);

	fly_body_release(body);
	fly_response_release(response);
}

__fly_static int __fly_response(
	fly_event_t *e,
	fly_response_t *response
){
	int send_len;
	char *send_start;

	if (e == NULL)
		return -1;
	if (response == NULL)
		return -1;
	if (response->pool == NULL)
		return -1;

	if (__fly_response_required_header(response) == -1)
		goto response_500;

	send_start = __fly_response_raw(response, &send_len);
	return __fly_send(e->fd, send_start, send_len, 0);

response_500:
	fly_itm_response_t *itm;

	itm = fly_pballoc(response->pool, sizeof(fly_itm_response_t));
	itm->status_code = _500;
	itm->req = response->request;
	__fly_500_error(e, V1_1, itm);
	return -1;
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
	if (fly_response_release(res) == -1)
		return -1;
	if (fly_connect_release(con) == -1)
		return -1;

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
	if (fly_response_release(res) == -1)
		return -1;
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

__fly_static int __fly_response_logcontent(fly_response_t *response, fly_event_t *e, fly_logcont_t *lc)
{
#define __FLY_RESPONSE_LOGCONTENT_SUCCESS			1
#define __FLY_RESPONSE_LOGCONTENT_ERROR				-1
#define __FLY_RESPONSE_LOGCONTENT_OVERFLOW			0
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
		response->request->request_line != NULL ? response->request->request_line->request_line : FLY_RESPONSE_NONSTRING,
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

__fly_static int __fly_response_log(fly_response_t *res, fly_event_t *e)
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

__fly_static int __fly_send_until_header_blocking(fly_event_t *e, fly_response_t *response)
{
	e->event_data = (void *) response;
	e->read_or_write = FLY_WRITE;
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
					numsend = send(c_sockfd, __status_line+total, result-total, 0);
					if (FLY_BLOCKING(numsend)){
						if (__fly_send_until_header_blocking(e, response) == -1)
							return FLY_RESPONSE_ERROR;
						return FLY_RESPONSE_BLOCKING;
					}else if (numsend == -1)
						return FLY_RESPONSE_ERROR;

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
						numsend = send(c_sockfd, __header_line+total, result-total, 0);
						if (FLY_BLOCKING(numsend)){
							if (__fly_send_until_header_blocking(e, response) == -1)
								return FLY_RESPONSE_ERROR;
							return FLY_RESPONSE_BLOCKING;
						}else if (numsend == -1)
							return FLY_RESPONSE_ERROR;

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
					numsend = send(c_sockfd, FLY_CRLF+total, FLY_CRLF_LENGTH-total, 0);
					if (FLY_BLOCKING(numsend)){
						if (__fly_send_until_header_blocking(e, response) == -1)
							return -1;
						return FLY_RESPONSE_BLOCKING;
					}else if (numsend == -1)
						return FLY_RESPONSE_ERROR;
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

__fly_static int __fly_send_body_blocking(fly_event_t *e, fly_response_t *response)
{
	e->event_data = (void *) response;
	e->read_or_write = FLY_WRITE;
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

		sendnum = send(e->fd, buf, total, 0);
		if (FLY_BLOCKING(sendnum)){
			if (__fly_send_body_blocking(e, response) == -1)
				return FLY_RESPONSE_ERROR;
			return FLY_RESPONSE_SUCCESS;
		}else if (sendnum == -1)
			return FLY_RESPONSE_ERROR;
		total += sendnum;
	}

	*bfs = 0;
	response->send_ptr = NULL;
	response->fase = FLY_RESPONSE_RELEASE;
	return FLY_RESPONSE_SUCCESS;
}

int fly_response_content_event_handler(fly_event_t *e)
{
	fly_response_t *response;
	struct fly_response_content *rc;
	int c_sockfd;
	off_t offset;
	fly_encoding_type_t *enctype=NULL;

	rc = (struct fly_response_content *) e->event_data;

	if (e->expired){
		e->event_data = (void *) rc->request;
		return fly_request_timeout(e);
	}

	c_sockfd = rc->request->connect->c_sockfd;
	response = fly_response_init();
	if (fly_unlikely_null(response))
		return -1;

	response->request = rc->request;
	response->status_code = _200;
	response->version = V1_1;
	response->header = fly_header_init();
	response->encoding = rc->request->encoding;

	fly_add_content_length_from_stat(response->header, &rc->pf->fs);
	fly_add_content_etag(response->header, rc->pf);
	fly_add_date(response->header);
	fly_add_last_modified(response->header, rc->pf);
	fly_add_connection(response->header, KEEP_ALIVE);
	fly_add_content_encoding(response->header, rc->request->encoding);
	fly_add_content_type(response->header, rc->pf->mime_type);

	offset = 0;
	if (__fly_encode_do(response)){
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
		__de->encbuf = fly_e_buf_add(__de);
		__de->decbuf = fly_d_buf_add(__de);
		__de->fd = rc->pf->fd;
		__de->offset = offset;
		__de->count = rc->pf->fs.st_size;
		__de->event = e;
		__de->response = response;
		__de->send = send;
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

	e->event_data = (fly_response_t *) response;
	if (response->encoding == NULL){
		switch (fly_send_from_pf(e, c_sockfd, rc->pf, &offset, rc->pf->fs.st_size)){
		case FLY_RESPONSE_SUCCESS:
			break;
		case FLY_RESPONSE_BLOCKING:
			/* event register in fly_send_from_pf */
			return 0;
		case FLY_RESPONSE_ERROR:
			return -1;
		}
	}else{
		switch(fly_esend_body(e, response)){
		case FLY_RESPONSE_SUCCESS:
			break;
		case FLY_RESPONSE_ERROR:
			return -1;
		case FLY_RESPONSE_BLOCKING:
			return 0;
		}
	}

	/* release response resource */
	response->fase = FLY_RESPONSE_RELEASE;
	return __fly_after_response(e, response);
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
	if (response->body == NULL){
		rcbs = fly_default_content_by_stcode_from_event(e, response->status_code);
		if (rcbs){
			if (fly_add_content_length_from_fd(response->header, rcbs->fd) == -1)
				return -1;
			if (fly_add_content_type(response->header, rcbs->mime) == -1)
				return -1;
		}
	}

	switch (__fly_send_until_header(e, response)){
	case FLY_RESPONSE_SUCCESS:
		break;
	case FLY_RESPONSE_BLOCKING:
		/* event register in __fly_send_until_header */
		return 0;
	case FLY_RESPONSE_ERROR:
		return -1;
	}

	if (response->body){
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
	}else{
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

int fly_response_release(fly_response_t *response)
{
	if (response == NULL)
		return -1;

	if (response->request != NULL)
		response->request = NULL;
	if (response->header != NULL)
		fly_header_release(response->header);
	if (response->body != NULL)
		fly_body_release(response->body);

	return fly_delete_pool(&response->pool);
}

__fly_static int __fly_4xx_error_handler(fly_event_t *e)
{
	fly_itm_response_t *itm;

	itm = (fly_itm_response_t *) e->event_data;
	__fly_4xx_error(e, FLY_DEFAULT_HTTP_VERSION, itm);
	/* end_of_connection */
	fly_socket_release(e->fd);

	if (fly_event_unregister(e) == -1)
		return -1;

	return 0;
}
__fly_static int __fly_5xx_error_handler(fly_event_t *e)
{
	fly_itm_response_t *itm;

	itm = (fly_itm_response_t *) e->event_data;
	__fly_5xx_error(e, FLY_DEFAULT_HTTP_VERSION, itm);
	/* end_of_connection */
	fly_socket_release(e->fd);

	if (fly_event_unregister(e) == -1)
		return -1;

	return 0;
}

int fly_4xx_error_event(fly_event_t *e, fly_request_t *req, fly_stcode_t code)
{
	fly_itm_response_t *itm;
	itm = fly_pballoc(req->pool, sizeof(fly_itm_response_t));
	if (itm == NULL)
		return -1;
	itm->status_code = code;
	itm->req = req;

	e->read_or_write = FLY_WRITE;
	e->event_data = (void *) itm;
	/* close socket in 4xx event */
	e->flag = FLY_CLOSE_EV;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, __fly_4xx_error_handler);
	fly_event_socket(e);

	return fly_event_register(e);
}

int fly_5xx_error_event(fly_event_t *e, fly_request_t *req, fly_stcode_t code)
{
	fly_itm_response_t *itm;
	itm = fly_pballoc(req->pool, sizeof(fly_itm_response_t));
	if (itm == NULL)
		return -1;
	itm->status_code = code;
	itm->req = req;

	e->read_or_write = FLY_WRITE;
	e->event_data = (void *) itm;
	/* close socket in 5xx event */
	e->flag = FLY_CLOSE_EV;
	e->tflag = FLY_INHERIT;
	e->eflag = 0;
	FLY_EVENT_HANDLER(e, __fly_5xx_error_handler);
	fly_event_socket(e);

	return fly_event_register(e);
}

__fly_static int __fly_until_header_handler(fly_event_t *e)
{
	fly_response_t *res;

	res = (fly_response_t *) e->event_data;
	if (__fly_send_until_header(e, res) == -1)
		return -1;
	if (__fly_response_log(res, e) == -1)
		return -1;

	/* release response content. */
	if (fly_response_release(res) == -1)
		return -1;

	return fly_event_unregister(e);
}

int fly_304_event(__unused fly_event_t *e, __unused struct fly_response_content *rc)
{
	fly_response_t *res;
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _304;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_content_etag(res->header, pf);
	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _400;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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

int fly_404_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _404;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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

int fly_405_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _405;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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

int fly_414_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _414;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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

int fly_415_event(fly_event_t *e, fly_request_t *req)
{
	fly_response_t *res;
	res= fly_response_init();
	res->header = fly_header_init();
	res->version = V1_1;
	res->status_code = _415;
	res->request = req;
	res->encoded = false;
	res->offset = 0;
	res->byte_from_start = 0;

	fly_add_allow(res->header, req);
	fly_add_date(res->header);
	fly_add_server(res->header);
	fly_add_connection(res->header, KEEP_ALIVE);

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

inline static int __fly_encode_do(fly_response_t *res)
{
	return (res->encoding && res->encoding->actqty);
}
