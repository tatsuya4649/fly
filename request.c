#include <stdio.h>
#include <ctype.h>
#include "alloc.h"
#include "request.h"
#include "response.h"
#include "version.h"
#include "util.h"
#include "encode.h"
#include "mount.h"
#include "scheme.h"

int fly_request_disconnect_handler(fly_event_t *event);
int fly_request_timeout_handler(fly_event_t *event);
__fly_static int __fly_request_operation(fly_request_t *req,fly_reqlinec_t *request_line);

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
	req->request_line = NULL;		/* use request pool */
	req->header = NULL;				/* use hedaer pool */
	req->body = NULL;				/* use body pool */
	req->buffer = fly_pballoc(pool, FLY_BUFSIZE); /* usr request pool */
	if (fly_unlikely_null(req->buffer))
		return NULL;

	req->mime = NULL;				/* use request pool */
	req->encoding = NULL;			/* use request pool */
	req->language = NULL;			/* use request pool */
	req->charset = NULL;			/* use request pool */
	req->bptr = req->buffer;
	memset(req->buffer, 0, FLY_BUFSIZE);
	req->connect = conn;			/* use connect pool */
	req->fase = EFLY_REQUEST_FASE_REQUEST_LINE;
	req->ctx = conn->event->manager->ctx;

	return req;
}

int fly_request_release(fly_request_t *req)
{
	if (req == NULL)
		return -1;

	if (req->header && (fly_header_release(req->header) == -1))
		return -1;
	if (req->body && (fly_body_release(req->body) == -1))
		return -1;
	return fly_delete_pool(&req->pool);
}

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer)
{
	return buffer;
}

static inline bool __fly_ualpha(char c)
{
	return (c>=0x41 && c<=0x5A) ? true : false;
}

static inline bool __fly_lalpha(char c)
{
	return (c>=0x61 && c<=0x7A) ? true : false;
}

static inline bool __fly_alpha(char c)
{
	return (__fly_ualpha(c) || __fly_lalpha(c)) ? true : false;
}
static inline bool __fly_digit(char c)
{
	return (c>=0x30 && c<=0x39) ? true : false;
}
static inline bool __fly_alpha_digit(char c)
{
	return (__fly_alpha(c) || __fly_digit(c)) ? true : false;
}
static inline bool __fly_space(char c)
{
	return (c == 0x20) ? true : false;
}
static inline bool __fly_ht(char c)
{
	return (c == '\t' ? true : false);
}
static inline bool __fly_slash(char c)
{
	return (c == '/') ? true : false;
}
static inline bool __fly_dot(char c)
{
	return (c == '.') ? true : false;
}
static inline bool __fly_point(char c)
{
	return __fly_dot(c);
}
static inline bool __fly_cr(char c)
{
	return (c == 0xD) ? true : false;
}
static inline bool __fly_lf(char c)
{
	return (c == 0xA) ? true : false;
}
static inline bool __fly_colon(char c)
{
	return (c == ':') ? true : false;
}
static inline bool __fly_vchar(char c)
{
	return (c >= 0x21 && c <= 0x7E) ? true : false;
}
static inline bool __fly_zero(char c)
{
	return (c == '\0') ? true : false;
}
static inline bool __fly_atsign(char c)
{
	return (c == 0x40) ? true : false;
}

static inline bool __fly_question(char c)
{
	return (c == 0x3F) ? true : false;
}

static inline bool __fly_sub_delims(char c)
{
	return (
		c=='!' || c=='$' || c=='&' || c==0x27 || c=='(' || \
		c==')' || c=='*' || c=='+' || c==',' || c==';' || \
		c=='=' \
	) ? true : false;
}

static inline bool __fly_hexdigit(char c)
{
	return ((c>=0x30&&c<=0x39) || (c>=0x61&&c<=0x66) || (c>=0x41&&c<=0x46)) ? true : false;
}

static inline bool __fly_tchar(char c)
{
	return ( \
		c=='!' || c=='#' || c=='$' || c=='%' || c=='&' || \
		c==0x27 || c=='*' || c=='+' || c=='-' || c=='.' || \
		c=='^' || c=='_' || c=='`' || c=='|' || c=='~' || \
		__fly_digit(c) || __fly_alpha(c) || (c!=';' && __fly_vchar(c)) \
	) ? true : false;
}
static inline bool __fly_token(char c)
{
	return __fly_tchar(c);
}
static inline bool __fly_method(char c)
{
	return __fly_token(c);
}

static inline bool __fly_asterisk(char c)
{
	return c=='*' ? true : false;
}

static inline bool __fly_unreserved(char c)
{
	return (__fly_alpha(c) || __fly_digit(c) || \
		c=='=' || c=='.' || c=='_' || c==0x7E
	) ? true : false;
}

static inline bool __fly_pct_encoded(char **c)
{
	if (**c != '%')
		return false;
	if (!__fly_hexdigit(*(*c+1)))
		return false;
	if (!__fly_hexdigit(*(*c+2)))
		return false;

	*c += 3;
	return true;
}

static inline bool __fly_pchar(char **c)
{
	return (__fly_unreserved(**c) || __fly_colon(**c) ||	\
		__fly_sub_delims(**c) || __fly_atsign(**c) ||		\
		__fly_pct_encoded(c)								\
	) ? true : false;
}

static inline bool __fly_segment(char **c)
{
	return __fly_pchar(c);
}

static inline bool __fly_query(char **c)
{
	return (								\
		__fly_pchar(c) || __fly_slash(**c) || __fly_question(**c) \
	) ? true : false;
}

static bool __fly_hier_part(char **c)
{
	if (!(__fly_slash(**c) && __fly_slash(*(*c+1))))
		return false;
	return true;
}

static inline bool __fly_userinfo(char **c)
{
	return (__fly_unreserved(**c) || __fly_sub_delims(**c) || \
		__fly_colon(**c) || __fly_pct_encoded(c)			  \
	) ? true : false;
}

static inline bool __fly_port(char c)
{
	return __fly_digit(c);
}

static inline bool __fly_host(char **c)
{
	return __fly_alpha_digit(**c);
}

static bool __fly_http(char **c __unused)
{
	/* HTTP */
	if (!(**c == 0x48 && *(*c+1) == 0x54 && *(*c+2) == 0x54 && *(*c+3) == 0x50))
		return false;
	*c += 4;
	return true;
}

__fly_static enum method_type __fly_request_method(fly_reqlinec_t *c)
{
	fly_http_method_t *__m;
	for (__m=methods; __m->name; __m++)
		if (*c == *__m->name)
			goto parse_method;

	return -1;
parse_method:
	char *ptr;
	ptr = __m->name;
	while(*ptr)
		if (*ptr++ != *c++)
			return -1;

	return __m->type;
}

__fly_static int __fly_parse_reqline(fly_request_t *req, fly_reqlinec_t *request_line)
{
	fly_reqlinec_t *ptr, *method, *http_version, *request_target;
	enum method_type method_type;
	enum {
		INIT,
		METHOD,
		METHOD_SPACE,
		ORIGIN_FORM,
		ORIGIN_FORM_QUESTION,
		ORIGIN_FORM_QUERY,
		ABSOLUTE_FORM,
		ABSOLUTE_FORM_COLON,
		ABSOLUTE_FORM_QUESTION,
		ABSOLUTE_FORM_QUERY,
		AUTHORITY_FORM,
		AUTHORITY_FORM_USERINFO,
		AUTHORITY_FORM_ATSIGN,
		AUTHORITY_FORM_HOST,
		AUTHORITY_FORM_COLON,
		AUTHORITY_FORM_PORT,
		ASTERISK_FORM,
		END_REQUEST_TARGET,
		REQUEST_TARGET_SPACE,
		HTTP_NAME,
		HTTP_SLASH,
		HTTP_VERSION_MAJOR,
		HTTP_VERSION_POINT,
		HTTP_VERSION_MINOR,
		END_HTTP_VERSION,
		CR,
		LF,
		SUCCESS,
	} status;
	ptr = request_line;

	status = INIT;
	while(true){
		switch(status){
			case INIT:
				if (__fly_method(*ptr)){
					method = ptr;
					status = METHOD;
					continue;
				}

				goto error;
			case METHOD:
				if (__fly_method(*ptr))	break;
				else if (__fly_space(*ptr)){
					/* request method */
					req->request_line->method = fly_match_method_name_with_end(method, FLY_SPACE);
					if (req->request_line->method == NULL)
						goto error;
					status = METHOD_SPACE;
					continue;
				}

				goto error;
			case METHOD_SPACE:
				if (__fly_space(*ptr)) break;

				request_target = ptr;
				method_type = __fly_request_method(method);
				switch (method_type){
				case CONNECT:
					status = AUTHORITY_FORM;
					continue;
				default: break;;
				}

				if (__fly_asterisk(*ptr) && method_type==OPTIONS){
					status = ASTERISK_FORM;
					continue;
				}else if (__fly_slash(*ptr)){
					status = ORIGIN_FORM;
					continue;
				}else{
					status = ABSOLUTE_FORM;
					continue;
				}

				goto error;
			case ORIGIN_FORM:
				if (__fly_slash(*ptr))	break;
				else if (__fly_segment(&ptr)) continue;
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}
				if (__fly_question(*ptr)){
					status = ORIGIN_FORM_QUESTION;
					break;
				}
				goto error;
			case ORIGIN_FORM_QUESTION:
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}
				if (__fly_query(&ptr)){
					status = ORIGIN_FORM_QUERY;
					break;
				}

				goto error;
			case ORIGIN_FORM_QUERY:
				if (__fly_query(&ptr))	break;
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}
				goto error;

			case ABSOLUTE_FORM:
				if (!is_fly_scheme(&ptr, ':'))
					goto error;
				if (__fly_colon(*ptr)){
					status = ABSOLUTE_FORM_COLON;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_COLON:
				if (!__fly_hier_part(&ptr))
					goto error;
				if (__fly_question(*ptr)){
					status = ABSOLUTE_FORM_QUESTION;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_QUESTION:
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}
				if (__fly_query(&ptr)){
					status = ABSOLUTE_FORM_QUERY;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_QUERY:
				if (__fly_query(&ptr))	break;
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}
				goto error;
			case AUTHORITY_FORM:
				if (__fly_userinfo(&ptr)){
					status = AUTHORITY_FORM_USERINFO;
					break;
				}
				if (__fly_host(&ptr)){
					status = AUTHORITY_FORM_HOST;
					break;
				}
				goto error;
			case AUTHORITY_FORM_USERINFO:
				if (__fly_userinfo(&ptr))	break;

				if (__fly_atsign(*ptr)){
					status = AUTHORITY_FORM_ATSIGN;
					break;
				}

				goto error;
			case AUTHORITY_FORM_ATSIGN:
				if (__fly_host(&ptr)){
					status = AUTHORITY_FORM_HOST;
					break;
				}

				goto error;
			case AUTHORITY_FORM_HOST:
				if (__fly_host(&ptr))		break;
				if (__fly_colon(*ptr)){
					status = AUTHORITY_FORM_COLON;
					break;
				}

				goto error;
			case AUTHORITY_FORM_COLON:
				if (__fly_port(*ptr)){
					status = AUTHORITY_FORM_PORT;
					break;
				}
				goto error;
			case AUTHORITY_FORM_PORT:
				if (__fly_port(*ptr))	break;
				if (__fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}

				goto error;
			case END_REQUEST_TARGET:
				/* add request/request_line/uri */
				fly_uri_set(req, request_target, ptr-request_target);
				status = REQUEST_TARGET_SPACE;
				continue;
			case REQUEST_TARGET_SPACE:
				if (__fly_space(*ptr))	break;
				status = HTTP_NAME;
				continue;
			case HTTP_NAME:
				if (!__fly_http(&ptr))
					goto error;
				if (__fly_slash(*ptr)){
					status = HTTP_SLASH;
					break;
				}
				goto error;
			case HTTP_SLASH:
				if (__fly_digit(*ptr)){
					http_version = ptr;
					status = HTTP_VERSION_MAJOR;
					break;
				}
				goto error;
			case HTTP_VERSION_MAJOR:
				if (__fly_point(*ptr)){
					status = HTTP_VERSION_POINT;
					break;
				}
				if (__fly_cr(*ptr)){
					status = END_HTTP_VERSION;
					break;
				}
				goto error;
			case HTTP_VERSION_POINT:
				if (__fly_digit(*ptr)){
					status = HTTP_VERSION_MINOR;
					break;
				}
				goto error;
			case HTTP_VERSION_MINOR:
				if (__fly_cr(*ptr)){
					status = END_HTTP_VERSION;
					break;
				}
				goto error;
			case END_HTTP_VERSION:
				/* add http version */
				req->request_line->version = fly_match_version_with_end(http_version, FLY_CR);
				if (!req->request_line->version)
					goto error;
				status = CR;
				continue;
			case CR:
				if (__fly_lf(*ptr)){
					status = LF;
					break;
				}
				goto error;
			case LF:
				status = SUCCESS;
				break;
			case SUCCESS:
				return 1;
			default:
				goto error;
				break;
		}
		ptr++;
	}
error:
	return -1;
}

__fly_static int __fly_request_operation(fly_request_t *req,fly_reqlinec_t *request_line)
{
	/* get request */
	int request_line_length;

	/* not ready for request line */
	if (strstr(request_line, "\r\n") == NULL)
		goto not_ready;

	request_line_length = strstr(request_line, "\r\n") - request_line;
	if (request_line_length >= FLY_REQUEST_LINE_MAX)
		goto error_414;

	req->request_line = fly_pballoc(req->pool, sizeof(fly_reqline_t));
	req->request_line->request_line = fly_pballoc(req->pool, sizeof(fly_reqlinec_t)*(request_line_length+1));

	if (fly_unlikely_null(req->request_line))
		goto error_500;
	if (fly_unlikely_null(req->request_line->request_line))
		goto error_500;

	memcpy(req->request_line->request_line, request_line, request_line_length);
	/* get total line */
	req->request_line->request_line[request_line_length] = '\0';

	/* request line parse check */
	if (__fly_parse_reqline(req, request_line) == -1)
		goto error_400;

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
//error_501:
//	/* Request line Too long */
//	return FLY_REQUEST_ERROR(501);
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
	return (__fly_alpha_digit(c) || __fly_char_match(c, "!@#$%^&()?-=_+|\\`~/;*[]<>{}|'\".,?")) ? 1 : 0;
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
			else if (__fly_alpha_digit(*ptr)){
				now = VALUE;
				*value = ptr;
				prev = GAP;
				continue;
			}else if (__fly_cr(*ptr)){
				now = CR;
				prev = GAP;
				continue;
			}else if (__fly_lf(*ptr)){
				now = LF;
			}else if (__fly_zero(*ptr))
				goto in_the_middle;
			else
				goto in_the_middle;

			prev = GAP;
			break;
		case GAP_SPACE:
			if (__fly_header_gap_usable(*ptr))
				;
			else if (__fly_header_value_usable(*ptr)){
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
			else if (prev == GAP   && __fly_cr(*ptr))
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
		request->bptr += recvlen;
	}
end_of_connection:
	*request->bptr = '\0';
	return 0;
continuation:
	*request->bptr = '\0';
	return 1;
error:
	return -1;
}

int fly_request_disconnect_handler(fly_event_t *event)
{
	__unused fly_request_t *req;
	fly_sock_t discon_sock;

	req = (fly_request_t *) event->event_data;
	discon_sock = event->fd;

	/* TODO: release some resources */
	if (fly_event_unregister(event) == -1)
		return -1;
	if (fly_socket_close(discon_sock, FLY_SOCK_CLOSE) == -1)
		return -1;

	return 0;
}

int fly_request_timeout_handler(fly_event_t *event)
{
	fly_request_t *req;
	fly_connect_t *conn;
	req = (fly_request_t *) event->event_data;

	conn = req->connect;

	/* release some resources */
	/* close socket and release resources. */
	if (fly_request_release(req) == -1)
		return -1;
	if (fly_event_unregister(event) == -1)
		return -1;
	if (fly_connect_release(conn) == -1)
		return -1;
	return 0;
}

#include "cache.h"
int fly_request_event_handler(fly_event_t *event)
{
	fly_request_t *request;
	fly_response_t *response;
	fly_reqlinec_t *request_line_ptr;
	char *header_ptr;
	fly_body_t *body;
	fly_bodyc_t *body_ptr;
	fly_route_reg_t *route_reg;
	fly_route_t *route;
	fly_mount_t *mount;
	struct fly_mount_parts_file *pf;
	__unused fly_request_state_t state;
	fly_request_fase_t fase;

	state = (fly_request_state_t) event->event_state;
	fase = (fly_request_fase_t) event->event_fase;
	request = (fly_request_t *) event->event_data;

	if (is_fly_event_timeout(event))
		goto timeout;

	fly_event_fase(event, REQUEST_LINE);
	fly_event_state(event, RECEIVE);
	switch (fly_request_receive(event->fd, request)){
	case -1:
		goto error;
	case 0:
		/* end of connection */
		goto disconnection;
	}

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
	request_line_ptr = fly_get_request_line_ptr(request->buffer);
	if (request_line_ptr == NULL)
		goto response_500;
	switch(__fly_request_operation(request, request_line_ptr)){
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
	header_ptr = fly_get_header_lines_ptr(request->buffer);
	if (header_ptr == NULL)
		goto continuation;

	switch (fly_reqheader_operation(request, header_ptr)){
	case __REQUEST_HEADER_ERROR:
		goto response_400;
	case __REQUEST_HEADER_IN_THE_MIDDLE:
		goto continuation;
	case __REQUEST_HEADER_SUCCESS:
		break;
	}

	/* accept encoding parse */
	if (fly_accept_encoding(request) == -1)
		goto error;

	/* accept mime parse */
	if (fly_accept_mime(request) == -1)
		goto error;

	/* accept charset parse */
	if (fly_accept_charset(request) == -1)
		goto error;

	/* accept language parse */
	if (fly_accept_language(request) == -1)
		goto error;

	/* check of having body */
	size_t content_length;
	content_length = fly_content_length(request->header);
	if (!content_length)
		goto __fase_end_of_parse;

	/* parse body */
__fase_body:
	fly_event_fase(event, BODY);
	body = fly_body_init();
	if (body == NULL)
		goto error;
	request->body = body;
	body_ptr = fly_get_body_ptr(request->buffer);
	/* content-encoding */
	fly_hdr_value *ev;
	fly_encoding_type_t *et;
	ev = fly_content_encoding(request->header);
	if (ev){
		/* not supported encoding */
		et = fly_supported_content_encoding(ev);
		if (!et)
			goto response_415;
		if (fly_decode_body(body_ptr, et, body, content_length) == NULL)
			goto error;
	}else{
		if (fly_body_setting(body, body_ptr, content_length) == -1)
			goto error;
	}


__fase_end_of_parse:
	fly_event_fase(event, RESPONSE);
	/* Success parse request */
	enum method_type __mtype;
	__mtype = request->request_line->method->type;
	route_reg = event->manager->ctx->route_reg;
	/* search from registerd route uri */
	route = fly_found_route(route_reg, request->request_line->uri.ptr, __mtype);
	mount = event->manager->ctx->mount;
	if (route == NULL){
		int found_res;
		/* search from uri */
		found_res = fly_found_content_from_path(mount, &request->request_line->uri, &pf);
		if (__mtype != GET && found_res){
			goto response_405;
		}else if (__mtype == GET && found_res)
			goto response_path;
		goto response_404;
	}

	/* defined handler */
	response = route->function(request);
	if (response == NULL)
		goto response_500;

	response->request = request;
	goto response;
/* TODO: error response event memory release */
response_400:
	fly_4xx_error_event(event, request, _400);
	return 0;
response_404:
	if (fly_404_event(event, request) == -1)
		goto error;
	return 0;
response_405:
	if (fly_405_event(event, request) == -1)
		goto error;
	return 0;
response_414:
	fly_4xx_error_event(event, request, _414);
	return 0;
response_415:
	fly_4xx_error_event(event, request, _415);
	return 0;
response_500:
	fly_5xx_error_event(event, request, _500);
	return 0;
response_501:
	fly_5xx_error_event(event, request, _501);
	return 0;

/* continuation event publish. */
continuation:
	event->event_state = (void *) EFLY_REQUEST_STATE_CONT;
	event->read_or_write = FLY_READ;
	event->flag = FLY_MODIFY;
	FLY_EVENT_HANDLER(event, fly_request_event_handler);
	event->tflag = FLY_INHERIT;
	event->available = false;
	fly_event_socket(event);
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

disconnection:
	event->event_state = (void *) EFLY_REQUEST_STATE_END;
	event->read_or_write = FLY_READ;
	event->flag = FLY_CLOSE_EV | FLY_MODIFY;
	FLY_EVENT_HANDLER(event, fly_request_disconnect_handler);
	event->available = false;
	fly_event_socket(event);
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

/* expired */
timeout:
	if (fly_request_timeout(event) == -1)
		goto error;
	return 0;

response_path:

	if (fly_if_none_match(request->header, pf))
		goto response_304;
	if (fly_if_modified_since(request->header, pf))
		goto response_304;
	struct fly_response_content *rc;
	rc = fly_pballoc(request->pool, sizeof(struct fly_response_content));
	if (fly_unlikely_null(rc))
		goto error;
	rc->pf = pf;
	rc->request = request;
	event->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	event->read_or_write = FLY_WRITE;
	event->flag = FLY_MODIFY;
	event->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(event, fly_response_content_event_handler);
	event->available = false;
	event->event_data = (void *) rc;
	fly_event_socket(event);
	if (fly_event_register(event) == -1)
		goto error;

	return 0;

response_304:
	struct fly_response_content *rc_304;

	rc_304 = fly_pballoc(request->pool, sizeof(struct fly_response_content));
	if (fly_unlikely_null(rc_304))
		goto error;
	rc_304->pf = pf;
	rc_304->request = request;
	if (fly_304_event(event, rc_304) == -1)
		goto error;
	return 0;

response:
	event->event_state = (void *) EFLY_REQUEST_STATE_RESPONSE;
	event->read_or_write = FLY_WRITE;
	event->flag = FLY_MODIFY;
	event->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(event, fly_response_event);
	event->available = false;
	event->event_data = (void *) response;
	fly_event_socket(event);
	if (fly_event_register(event) == -1)
		goto error;

	return  0;

error:
	return -1;
}


int fly_request_timeout(fly_event_t *event)
{
	event->event_state = (void *) EFLY_REQUEST_STATE_TIMEOUT;
	event->read_or_write = FLY_WRITE;
	event->flag = FLY_CLOSE_EV | FLY_MODIFY;
	event->tflag = FLY_INHERIT;
	FLY_EVENT_HANDLER(event, fly_request_timeout_handler);
	event->available = false;
	fly_event_socket(event);
	return fly_event_register(event);
}
