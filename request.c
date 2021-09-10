#include <stdio.h>
#include <ctype.h>
#include "alloc.h"
#include "request.h"
#include "response.h"
#include "version.h"
#include "util.h"

int fly_request_disconnect_handler(fly_event_t *event);
int fly_request_timeout_handler(fly_event_t *event);
fly_request_t *fly_request_init(fly_connect_t *conn)
{
	fly_pool_t *pool;
	fly_request_t *req;
	pool = fly_create_pool(FLY_REQUEST_POOL_SIZE);
	if (pool == NULL)
		return NULL;
	req = fly_pballoc(pool, sizeof(fly_request_t));
	if (req == NULL)
		return NULL;

	req->pool = pool;
	req->request_line = NULL;
	req->header = NULL;
	req->body = NULL;
	req->buffer = fly_pballoc(pool, FLY_BUFSIZE);
	if (req->buffer == NULL)
		return NULL;
	req->bptr = req->buffer;
	memset(req->buffer, 0, FLY_BUFSIZE);
	req->connect = conn;
	req->fase = EFLY_REQUEST_FASE_REQUEST_LINE;

	return req;
}

int fly_request_release(fly_request_t *req)
{
	if (req->header != NULL)
		fly_delete_pool(&req->header->pool);
	return fly_delete_pool(&req->pool);
}

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer)
{
	return buffer;
}

__fly_static inline int __fly_alpha(char c)
{
	if ((c>='a' && c<='z') || (c>='A' && c<='Z'))
		return 1;
	else
		return 0;
}
__fly_static inline int __fly_number(char c)
{
	if (c>='0' && c<='9')
		return 1;
	else
		return 0;
}
__fly_static inline int __fly_alpha_number(char c)
{
	return (__fly_alpha(c) || __fly_number(c)) ? 1 : 0;
}
__fly_static inline int __fly_space(char c)
{
	return (c == ' ') ? 1 : 0;
}
__fly_static inline int __fly_ht(char c)
{
	return (c == '\t' ? 1 : 0);
}
__fly_static inline int __fly_slash(char c)
{
	return (c == '/') ? 1 : 0;
}
__fly_static inline int __fly_dot(char c)
{
	return (c == '.') ? 1 : 0;
}
__fly_static inline int __fly_cr(char c)
{
	return (c == '\r') ? 1 : 0;
}
__fly_static inline int __fly_lf(char c)
{
	return (c == '\n') ? 1 : 0;
}
__fly_static inline int __fly_colon(char c)
{
	return (c == ':') ? 1 : 0;
}
__unused __fly_static inline int __fly_bracket(char c)
{
	return (c == '[' || c == ']') ? 1 : 0;
}
__unused __fly_static inline int __fly_gtlt(char c)
{
	return (c == '<' || c == '>') ? 1 : 0;
}
__unused __fly_static inline int __fly_equal(char c)
{
	return (c == '=') ? 1 : 0;
}
__fly_static inline int __fly_vchar(char c)
{
	return (c >= 0x21 && c <= 0x7E) ? 1 : 0;
}
__fly_static inline int  __fly_zero(char c)
{
	return (c == '\0') ? 1 : 0;
}

//__fly_static int __fly_obs_text(char c)
//{
//	return (c >= 0x80 && c <= 0xFF) ? 1 : 0;
//}

__fly_static int __fly_parse_reqline(fly_reqlinec_t *request_line)
{
	fly_reqlinec_t *ptr;
	enum reqtype{
		INIT,
		METHOD,
		METHOD_SPACE,
		URI,
		URI_SPACE,
		VERSION_PROTOCOL,
		VERSION_MAJOR,
		VERSION_MINOR,
		CRLF,
		SPACE,
	} now, prev;
	ptr = request_line;

	now = INIT;
	while(1){
		switch(now){
			case INIT:
				if (!__fly_alpha(*ptr))
					goto error;
				else
					now = METHOD;
				prev = INIT;
				break;
			case METHOD:
				if (__fly_alpha(*ptr))
					;
				else if (__fly_space(*ptr)){
					now = METHOD_SPACE;
				}else
					goto error;

				prev = METHOD;
				break;
			case METHOD_SPACE:
				if (__fly_space(*ptr))
					;
				else if (__fly_alpha(*ptr) || __fly_number(*ptr) || __fly_slash(*ptr))
					now = URI;
				else
					goto error;
				prev = METHOD_SPACE;
				break;
			case URI:
				if (__fly_alpha(*ptr) || __fly_number(*ptr) || __fly_slash(*ptr))
					;
				else if (__fly_space(*ptr))
					now = URI_SPACE;
				else
					goto error;
				prev = URI;
				break;
			case URI_SPACE:
				if (__fly_space(*ptr))
					;
				else if (__fly_alpha(*ptr))
					now = VERSION_PROTOCOL;
				else
					goto error;
				prev = URI_SPACE;
				break;
			case VERSION_PROTOCOL:
				if (__fly_alpha(*ptr))
					;
				else if (prev == VERSION_PROTOCOL && !__fly_slash(*(ptr-1)) && __fly_slash(*ptr))
					now = VERSION_MAJOR;
				else
					goto error;

				prev = VERSION_PROTOCOL;
				break;
			case VERSION_MAJOR:
				if (__fly_number(*ptr))
					;
				else if (prev == VERSION_MAJOR && !__fly_dot(*(ptr-1)) && __fly_dot(*ptr))
					now = VERSION_MINOR;
				else
					goto error;

				prev = VERSION_MAJOR;
				break;
			case VERSION_MINOR:
				if (prev != VERSION_MINOR && __fly_number(*ptr))
					;
				else if (prev == VERSION_MINOR && __fly_cr(*ptr))
					now = CRLF;
				else
					goto error;

				prev = VERSION_MINOR;
				break;
			case CRLF:
				if (__fly_cr(*(ptr-1)) && __fly_lf(*ptr))
					;
				else
					goto error;

				prev = CRLF;
				goto end;
				break;
			case SPACE:
				break;
			default:
				goto error;
				break;
		}
		ptr++;
	}
end:
	return 0;
error:
	return -1;
}

__fly_static int __fly_parse_request_line(fly_pool_t *pool, __unused int c_sock, fly_reqline_t *req)
{
	fly_reqlinec_t *request_line;
    char *method;
    char *space;

	if (pool == NULL || req == NULL)
		return -1;

	request_line = req->request_line;
    space = strstr(request_line, " ");
    /* method only => response 400 Bad Request */
    if (space == NULL)
        return FLY_ERROR(400);

    method = fly_pballoc(pool, space-request_line+1);
	/* memory alloc error */
	if (method == NULL)
		return FLY_ERROR(500);

    memcpy(method, request_line, space-request_line);
    method[space-request_line] = '\0';
    req->method = fly_match_method_name(method);
	/* no match method */
    if (req->method == NULL)
        return FLY_ERROR(400);

    char *uri_start = space+1;
    char *next_space = strstr(uri_start," ");
	/* not found http version */
	if (next_space == NULL)
        return FLY_ERROR(400);

	/* too long uri */
	if (next_space-uri_start >= FLY_REQUEST_URI_MAX)
		return FLY_ERROR(414);

    req->uri.uri = fly_pballoc(pool, next_space-uri_start+1);
	if (req->uri.uri == NULL)
		return FLY_ERROR(500);

    memcpy(req->uri.uri, space+1, next_space-uri_start);
    req->uri.uri[next_space-uri_start] = '\0';

    char *version_start = next_space+1;
    req->version = fly_match_version(version_start);
	/* unmatch version */
    if (req->version == NULL)
		return FLY_ERROR(400);

    char *slash_p = strchr(req->version->full,'/');
	/* http version number not found */
    if (slash_p == NULL)
		return FLY_ERROR(400);

    req->version->number = slash_p + 1;
    return 0;
}

int fly_request_operation(int c_sock, fly_pool_t *pool,fly_reqlinec_t *request_line, fly_request_t *req)
{
    /* get request */
    int request_line_length;

	/* not ready for request line */
	if (strstr(request_line, "\r\n") == NULL)
		goto not_ready;

	/* request line parse check */
	if (__fly_parse_reqline(request_line) == -1)
		goto error_400;

    request_line_length = strstr(request_line, "\r\n") - request_line;
	if (request_line_length >= FLY_REQUEST_LINE_MAX)
		goto error_501;

	req->request_line = fly_pballoc(pool, sizeof(fly_reqline_t));
    req->request_line->request_line = fly_pballoc(pool, sizeof(fly_reqlinec_t)*(request_line_length+1));
	if (req->request_line == NULL)
		goto error_500;
	if (req->request_line->request_line == NULL)
		goto error_500;

    memcpy(req->request_line->request_line, request_line, request_line_length);
    /* get total line */
    req->request_line->request_line[request_line_length] = '\0';

	switch(__fly_parse_request_line(pool, c_sock, req->request_line)){
	case 0:
		return 0;
	case FLY_ERROR(400):
		goto error_400;
	case FLY_ERROR(414):
		goto error_414;
	case FLY_ERROR(500):
		goto error_500;
	default:
		goto error_500;
	}
error_400:
	/* Bad Request(result of Parse) */
	return FLY_REQUEST_ERROR(400);
error_414:
	/* URI Too Long */
	return FLY_REQUEST_ERROR(414);
//error_not_found_request_line:
//	return FLY_REQUEST_ERROR(400);
error_500:
	/* Server Error */
	return FLY_REQUEST_ERROR(500);
error_501:
	/* Request line Too long */
	return FLY_REQUEST_ERROR(501);
not_ready:
	return FLY_REQUEST_NOREADY;
}


__fly_static int __fly_char_match(int c, const char *string)
{
	if (string == NULL)
		return 0;
	for (int i=0; i<(int) strlen(string); i++){
		if (c == string[i])
			return 1;
	}
	return 0;
}
__fly_static int __fly_header_name_usable(int c)
{
	return (__fly_alpha_number(c) || __fly_char_match(c, "!@#$%^&()?-=_+|\\`~/;*[]<>{}|'\".,?")) ? 1 : 0;
}
__fly_static int __fly_header_gap_usable(int c)
{
	return (__fly_space(c) || __fly_ht(c)) ? 1 : 0;
}
__fly_static int __fly_header_value_usable(int c)
{
	return __fly_vchar(c) || __fly_space(c) || __fly_ht(c);
}
__fly_static int __fly_end_of_header(char *ptr)
{
	if (__fly_lf(*ptr))
		return 1;
	else if (__fly_cr(*ptr)){
		return 1;
	}else
		return 0;
}

enum __fly_parse_type_result_type{
	_FLY_PARSE_SUCCESS,
	_FLY_PARSE_ERROR,
	_FLY_PARSE_FATAL,
	_FLY_PARSE_ITM,		/* in the middle */
	_FLY_PARSE_END_OF_HEADER
};
struct __fly_parse_header_line_result{
	fly_buffer_t *ptr;
	enum __fly_parse_type_result_type type;
};

__fly_static int __fly_parse_header_line(fly_buffer_t *header, struct __fly_parse_header_line_result *res,char **name, int *name_len,char **value, int *value_len)
{
	enum {
		INIT,
		NAME,
		GAP,
		GAP_SPACE,
		VALUE,
		CR,
		LF,
		NEXT
	} now, prev;

	fly_buffer_t *ptr = header;

	now = INIT;
	*name_len = 0;
	*value_len = 0;
	while(1){
		switch(now){
		case INIT:
			if (!__fly_header_name_usable(*ptr)){
				if (__fly_end_of_header(ptr))
					goto end_of_header;
				else if (__fly_zero(*ptr))
					goto in_the_middle;
				else
					goto error;
			}

			now = NAME;
			*name = ptr;
			prev = INIT;
			continue;
		case NAME:
			if (__fly_header_name_usable(*ptr))
				;
			else if (__fly_colon(*ptr)){
				now = GAP;
				prev = NAME;
				continue;
			}else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto error;

			(*name_len)++;
			prev = NAME;
			break;
		case GAP:
			if (__fly_colon(*ptr))
				;
			else if (__fly_header_gap_usable(*ptr))
				now = GAP_SPACE;
			else if (__fly_alpha_number(*ptr)){
				now = VALUE;
				*value = ptr;
				prev = GAP;
				continue;
			}else if (__fly_cr(*ptr))
				now = CR;
			else if (__fly_lf(*ptr))
				now = LF;
			else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto in_the_middle;

			prev = GAP;
			break;
		case GAP_SPACE:
			if (__fly_header_gap_usable(*ptr))
				;
			else if (__fly_alpha_number(*ptr)){
				now = VALUE;
				*value = ptr;
				prev = GAP_SPACE;
				continue;
			}else if (__fly_cr(*ptr))
				now = CR;
			else if (__fly_lf(*ptr))
				now = LF;
			else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto error;
			prev = GAP_SPACE;
			break;
		case VALUE:
			if (__fly_header_value_usable(*ptr))
				;
			else if (__fly_cr(*ptr)){
				now = CR;
				prev = VALUE;
				continue;
			}else if (__fly_lf(*ptr)){
				now = LF;
				prev = VALUE;
				continue;
			}else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto error;

			(*value_len)++;
			prev = VALUE;
			break;
		case CR:
			if (prev == VALUE && __fly_cr(*ptr))
				now = LF;
			else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto error;

			prev = CR;
			break;
		case LF:
			if (__fly_zero(*ptr))
				goto in_the_middle;

			now = NEXT;

			prev = LF;
			break;
		case NEXT:
			prev = NEXT;
			goto end_line;
		default:
			goto error;
		}
		ptr++;
	}
end_line:
	res->ptr = ptr;
	res->type = _FLY_PARSE_SUCCESS;
	return 1;
end_of_header:
	res->ptr = ptr;
	res->type = _FLY_PARSE_END_OF_HEADER;
	return 1;
error:
	if (strchr(ptr, FLY_LF) == NULL){
		res->ptr = ptr;
		res->type = _FLY_PARSE_FATAL;
		return -1;
	}else{
		while( !__fly_lf(*ptr++) )
			;
		res->ptr = ptr;
		res->type = _FLY_PARSE_ERROR;
		return -1;
	}
in_the_middle:
	res->ptr = ptr;
	res->type = _FLY_PARSE_ITM;
	return 0;
}
__fly_static int __fly_parse_header(fly_hdr_ci *ci, fly_buffer_t *header)
{
	enum {
		HEADER_LINE,
		END
	} now;

	fly_buffer_t *ptr = header;
	if (header == NULL)
		goto end;

	now = HEADER_LINE;
	while(1){
		switch(now){
		case HEADER_LINE:
			{
				char *name=NULL;
				char *value=NULL;
				int name_len, value_len;
				struct __fly_parse_header_line_result result;
				__fly_parse_header_line(ptr, &result, &name, &name_len, &value, &value_len);
				switch(result.type){
				case _FLY_PARSE_END_OF_HEADER:
					now = END;
					continue;
				case _FLY_PARSE_FATAL:
					goto fatal;
				case _FLY_PARSE_ERROR:
					break;
				case _FLY_PARSE_ITM:
					goto in_the_middle;
				case _FLY_PARSE_SUCCESS:
					if (fly_header_add(ci, name, name_len, value, value_len) == -1)
						return -1;
					break;
				default:
					continue;
				}
				ptr = result.ptr;
			}
			break;
		case END:
			goto end;
		default:
			goto error;
		}
	}

#define __REQUEST_HEADER_SUCCESS			1
#define __REQUEST_HEADER_IN_THE_MIDDLE		0
#define __REQUEST_HEADER_ERROR				-1
error:
	return __REQUEST_HEADER_ERROR;
fatal:
	return __REQUEST_HEADER_ERROR;
end:
	return __REQUEST_HEADER_SUCCESS;
in_the_middle:
	return __REQUEST_HEADER_IN_THE_MIDDLE;
}

int fly_reqheader_operation(fly_request_t *req, fly_buffer_t *header)
{
	fly_hdr_ci *rchain_info;
	rchain_info = fly_header_init();
	if (rchain_info == NULL)
		return -1;

	req->header = rchain_info;
	return __fly_parse_header(rchain_info, header);
}


int fly_request_receive(fly_sock_t fd, fly_request_t *request)
{
	if (request == NULL || request->buffer == NULL)
		return -1;

	int recvlen=0;

	while(1){
		/* buffer overflow */
		if (FLY_BUFSIZE-recvlen == 0)
			goto error;
		recvlen = recv(fd, request->bptr, FLY_BUFSIZE-recvlen, MSG_DONTWAIT);
		request->bptr += recvlen;
		switch(recvlen){
		case 0:
			goto end_of_connection;
		case -1:
			if (errno == EINTR)
				continue;
			else if (errno == EAGAIN || errno == EWOULDBLOCK)
				goto continuation;
			else
				goto error;
		default:
			break;
		}
	}
end_of_connection:
	request->bptr = '\0';
	return 0;
continuation:
	request->bptr = '\0';
	return 1;
error:
	return -1;
}

/* TODO */
/* Parse Request Line Event*/
int fly_rline_event_handler(fly_event_t *event);
/* Parse Request Header Event */
int fly_rheader_event_handler(fly_event_t *event);
/* Parse Request Body Event*/
int fly_rbody_event_handler(fly_event_t *event);


int fly_request_disconnect_handler(fly_event_t *event)
{
	__unused fly_request_t *req;
	fly_sock_t discon_sock;

	req = (fly_request_t *) event->event_data;
	discon_sock = event->fd;

	/* TODO: release some resources */
	if (fly_event_unregister(event) == -1)
		return -1;
	if (close(discon_sock) == -1)
		return -1;

	return 0;
}

int fly_request_timeout_handler(fly_event_t *event)
{
	__unused fly_request_t *req;
	fly_sock_t discon_sock;

	req = (fly_request_t *) event->event_data;
	discon_sock = event->fd;

	/* TODO: release some resources */
	if (fly_event_unregister(event) == -1)
		return -1;
	if (close(discon_sock) == -1)
		return -1;

	return 0;
}

int fly_request_event_handler(fly_event_t *event)
{
	fly_request_t *req;
	fly_reqlinec_t *request_line_ptr;
	char *header_ptr;
	fly_body_t *body;
	fly_bodyc_t *body_ptr;
	fly_route_reg_t *route_reg;
	fly_route_t *route;
	__unused fly_request_state_t state;
	__unused fly_request_fase_t fase;

	state = (fly_request_state_t) event->event_state;
	fase = (fly_request_fase_t) event->event_fase;
	req = (fly_request_t *) event->event_data;

	if (is_fly_event_timeout(event))
		goto timeout;

	fly_event_fase(event, REQUEST_LINE);
	fly_event_state(event, RECEIVE);
	switch (fly_request_receive(event->fd, req)){
	case -1:
		goto error;
	case 0:
		/* end of connection */
		goto disconnection;
	}

	printf("%s\n", req->buffer);
	switch(fase){
	case EFLY_REQUEST_FASE_INIT:
		break;
	case EFLY_REQUEST_FASE_REQUEST_LINE:
		goto __fase_request_line;
	case EFLY_REQUEST_FASE_HEADER:
		goto __fase_header;
	case EFLY_REQUEST_FASE_BODY:
		goto __fase_body;
	default:
		break;
	}
	/* parse request_line */
__fase_request_line:
	request_line_ptr = fly_get_request_line_ptr(req->buffer);
	if (request_line_ptr == NULL)
		goto error;
	switch(fly_request_operation(event->fd, req->pool, request_line_ptr, req)){
	case FLY_REQUEST_ERROR(400):
		goto response_400;
	case FLY_REQUEST_ERROR(414):
		goto response_414;
	case FLY_REQUEST_ERROR(500):
		goto response_500;
	case FLY_REQUEST_ERROR(501):
		goto response_501;
	/* not ready for request line */
	case FLY_REQUEST_NOREADY:
		goto continuation;
	default:
		break;
	}

	/* parse header */
__fase_header:
	fly_event_fase(event, HEADER);
	header_ptr = fly_get_header_lines_ptr(req->buffer);
	if (header_ptr == NULL)
		goto continuation;

	switch (fly_reqheader_operation(req, header_ptr)){
	case __REQUEST_HEADER_ERROR:
		goto response_400;
	case __REQUEST_HEADER_IN_THE_MIDDLE:
		goto continuation;
	case __REQUEST_HEADER_SUCCESS:
		break;
	}

	/* check of having body */
	if (fly_content_length(req->header) == 0)
		goto __fase_end_of_parse;

	/* parse body */
__fase_body:
	fly_event_fase(event, BODY);
	body = fly_body_init();
	if (body == NULL)
		goto error;
	req->body = body;
	body_ptr = fly_get_body_ptr(req->buffer);
	if (fly_body_setting(body, body_ptr) == -1)
		goto error;


__fase_end_of_parse:
	/* Success parse request */
	route_reg = event->manager->ctx->route_reg;
	route = fly_found_route(route_reg, req->request_line->uri.uri, req->request_line->method->type);
	if (route == NULL)
		goto response_404;


	/* TODO: success handler */
	return 0;
	goto continuation;

/* TODO: error response event memory release */
response_400:
	fly_4xx_error_event(event, event->fd, _400);
	goto error;
response_404:
	fly_4xx_error_event(event, event->fd, _404);
	goto error;
response_414:
	fly_4xx_error_event(event, event->fd, _414);
	goto error;
response_500:
	fly_5xx_error_event(event, event->fd, _500);
	goto error;
response_501:
	fly_5xx_error_event(event, event->fd, _501);
	goto error;

/* continuation event publish. */
continuation:
	event->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	event->flag = FLY_MODIFY;
	event->tflag = FLY_INHERIT;
	event->available = false;
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

disconnection:
	event->event_state = (void *) EFLY_REQUEST_STATE_END;
	event->flag = FLY_CLOSE_EV | FLY_MODIFY;
	event->handler = fly_request_disconnect_handler;
	event->available = false;
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

/* expired */
timeout:
	event->event_state = (void *) EFLY_REQUEST_STATE_TIMEOUT;
	event->flag = FLY_CLOSE_EV | FLY_MODIFY;
	event->tflag = FLY_INHERIT;
	event->handler = fly_request_timeout_handler;
	event->available = false;
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

error:
	return -1;
}
