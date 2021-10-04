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
__fly_static int __fly_request_operation(fly_request_t *req, fly_buffer_c *request_line);

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
	req->buffer = fly_buffer_init(
		pool,
		FLY_REQUEST_BUFFER_CHAIN_INIT_LEN,
		FLY_REQUEST_BUFFER_CHAIN_INIT_CHAIN_MAX,
		FLY_REQUEST_BUFFER_CHAIN_INIT_PER_LEN
	); /* usr request pool */
	if (fly_unlikely_null(req->buffer))
		return NULL;

	req->mime = NULL;				/* use request pool */
	req->encoding = NULL;			/* use request pool */
	req->language = NULL;			/* use request pool */
	req->charset = NULL;			/* use request pool */
	req->bptr = req->buffer->lchain;
	//memset(req->buffer, 0, FLY_BUFSIZE);
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

struct fly_buffer_chain *fly_get_request_line_buf(fly_buffer_t *__buf)
{
	if (fly_unlikely_null(__buf->chain))
		return NULL;
	return __buf->chain;
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

static inline bool __fly_sharp(char c)
{
	return c==0x23 ? true : false;
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

	*c += 2;
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

static inline void __fly_query_set(fly_request_t *req, fly_reqlinec_t *c)
{
	fly_reqlinec_t *ptr;

	ptr = c;
	req->request_line->query.ptr = c;

	req->request_line->query.len = 0;
	while(!__fly_space(*ptr) && !__fly_sharp(*ptr)){
		req->request_line->query.len++;
		ptr++;
	}
	return;
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
	fly_reqlinec_t *ptr=NULL, *method=NULL, *http_version=NULL, *request_target=NULL, *query=NULL;
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
				else if (__fly_segment(&ptr))	break;
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

				query = ptr;
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

				query = ptr;
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
				if (query)
					__fly_query_set(req, query);
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

__fly_static int __fly_request_operation(fly_request_t *req, fly_buffer_c *request_line)
{
	/* get request */
	size_t request_line_length;
	fly_buf_p rptr;

	/* not ready for request line */
	if ((rptr=fly_buffer_strstr_after(request_line, "\r\n")) == NULL)
		goto not_ready;

	request_line_length = fly_buffer_ptr_len(request_line->buffer, rptr, request_line->ptr);
//	request_line_length = strstr(request_line, "\r\n") - request_line;
	if (request_line_length >= FLY_REQUEST_LINE_MAX)
		goto error_414;

	req->request_line = fly_pballoc(req->pool, sizeof(fly_reqline_t));
	req->request_line->request_line = fly_pballoc(req->pool, sizeof(fly_reqlinec_t)*(request_line_length+1));

	if (fly_unlikely_null(req->request_line))
		goto error_500;
	if (fly_unlikely_null(req->request_line->request_line))
		goto error_500;

	fly_buffer_memcpy(req->request_line->request_line, request_line->ptr, request_line, request_line_length);

	/* get total line */
	req->request_line->request_line[request_line_length] = '\0';

	/* request line parse check */
	if (__fly_parse_reqline(req, req->request_line->request_line) == -1)
		goto error_400;
	return 0;
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


static inline bool __fly_header_field_name(char c)
{
	return __fly_token(c);
}
static inline bool __fly_ows(int c)
{
	return (__fly_space(c) || __fly_ht(c)) ? true : false;
}

static inline bool __fly_field_content(char c)
{
	return (__fly_vchar(c) || __fly_space(c) || __fly_ht(c)) \
		? true : false;
}

static inline bool __fly_header_field_value(char c)
{
	return __fly_field_content(c);
}
__fly_static bool __fly_end_of_header(char *ptr)
{
	if (__fly_lf(*ptr))
		return true;
	else if (__fly_cr(*ptr)){
		return true;
	}else
		return false;
}

enum __fly_parse_type_result_type{
	_FLY_PARSE_SUCCESS,
	_FLY_PARSE_ERROR,
	_FLY_PARSE_FATAL,
	_FLY_PARSE_ITM,		/* in the middle */
	_FLY_PARSE_END_OF_HEADER
};
struct __fly_parse_header_line_result{
	fly_buf_p ptr;
	enum __fly_parse_type_result_type type;
};

__fly_static int __fly_parse_header_line(fly_buffer_c **chain, fly_buf_p header, struct __fly_parse_header_line_result *res,char **field_name, int *field_name_len,char **field_value, int *field_value_len)
{
	enum {
		INIT,
		FIELD_NAME,
		COLON,
		OWS1,
		FIELD_VALUE,
		OWS2,
		CR,
		LF,
		NEXT
	} status;

	char *ptr = (char *) header;

	status = INIT;
	*field_name_len = 0;
	*field_value_len = 0;
	while(true){
		switch(status){
		case INIT:
			if (__fly_header_field_name(*ptr)){
				*field_name = ptr;
				status = FIELD_NAME;
				continue;
			}
			if (__fly_end_of_header(ptr))
				goto end_of_header;
			else if (__fly_zero(*ptr))
				goto in_the_middle;
			goto error;
		case FIELD_NAME:
			if (__fly_colon(*ptr)){
				status = COLON;
				break;
			}
			if (__fly_zero(*ptr))
				goto in_the_middle;
			if (__fly_header_field_name(*ptr)){
				(*field_name_len)++;
				break;
			}

			goto error;
		case COLON:
			if (__fly_ows(*ptr)){
				status = OWS1;
				continue;
			}
			goto error;
		case OWS1:
			if (__fly_ows(*ptr))	break;
			if (__fly_cr(*ptr)){
				status = CR;
				break;
			}
			if (__fly_lf(*ptr)){
				status = LF;
			}
			if (__fly_zero(*ptr))
				goto in_the_middle;

			if (__fly_header_field_value(*ptr)){
				status = FIELD_VALUE;
				*field_value = ptr;
				continue;
			}

			goto error;

		case FIELD_VALUE:
			if (__fly_cr(*ptr)){
				status = CR;
				break;
			}
			if (__fly_lf(*ptr)){
				status = LF;
				break;
			}

			if (__fly_zero(*ptr))
				goto in_the_middle;

			if (__fly_header_field_value(*ptr)){
				(*field_value_len)++;
				break;
			}
			goto error;

		case CR:
			if (__fly_lf(*ptr)){
				status = LF;
				break;
			}
			if (__fly_zero(*ptr))
				goto in_the_middle;

			goto error;
		case LF:
			status = NEXT;
			continue;
		case NEXT:
			goto end_line;
		default:
			goto error;
		}
		ptr = fly_update_chain_one(chain, ptr);
		if (!ptr)
			goto error;
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

__fly_static int __fly_parse_header(fly_hdr_ci *ci, fly_buffer_c *header, fly_buf_p hptr)
{
	enum {
		HEADER_LINE,
		END
	} status;

	fly_buffer_c *chain = header;
	fly_buf_p ptr = hptr;
	if (header == NULL)
		goto end;

	status = HEADER_LINE;
	while(true){
		switch(status){
		case HEADER_LINE:
			{
				char *name=NULL;
				char *value=NULL;
				int name_len, value_len;
				struct __fly_parse_header_line_result result;
				fly_buffer_c *__c = chain;
				__fly_parse_header_line(&chain, ptr, &result, &name, &name_len, &value, &value_len);
				switch(result.type){
				case _FLY_PARSE_END_OF_HEADER:
					status = END;
					continue;
				case _FLY_PARSE_FATAL:
					goto fatal;
				case _FLY_PARSE_ERROR:
					break;
				case _FLY_PARSE_ITM:
					goto in_the_middle;
				case _FLY_PARSE_SUCCESS:
					if (fly_header_addb(__c, ci, name, name_len, value, value_len) == -1)
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

int fly_reqheader_operation(fly_request_t *req, fly_buffer_c *header_chain, char *header_ptr)
{
	fly_hdr_ci *rchain_info;
	rchain_info = fly_header_init();
	if (rchain_info == NULL)
		return -1;

	req->header = rchain_info;
	return __fly_parse_header(rchain_info, header_chain, header_ptr);
}

int fly_request_receive(fly_sock_t fd, fly_connect_t *connect)
{
	fly_buffer_t *__buf;
	if (connect == NULL || connect->buffer == NULL)
		return -1;

	__buf = connect->buffer;
	if (fly_unlikely(__buf->chain_count == 0))
		return -1;

	int recvlen=0, total=0;
	while(true){
		if (FLY_CONNECT_ON_SSL(connect)){
			SSL *ssl = connect->ssl;

			recvlen = SSL_read(ssl, __buf->lunuse_ptr,  __buf->lunuse_len);
			switch(SSL_get_error(ssl, recvlen)){
			case SSL_ERROR_NONE:
				break;
			case SSL_ERROR_ZERO_RETURN:
				goto end_of_connection;
			case SSL_ERROR_WANT_READ:
				goto read_blocking;
			case SSL_ERROR_WANT_WRITE:
				goto write_blocking;
			case SSL_ERROR_SYSCALL:
				goto error;
			case SSL_ERROR_SSL:
				goto error;
			default:
				/* unknown error */
				goto error;
			}
		}else{
			recvlen = recv(fd, __buf->lunuse_ptr, __buf->lunuse_len, MSG_DONTWAIT);
			switch(recvlen){
			case 0:
				goto end_of_connection;
			case -1:
				if (errno == EINTR)
					continue;
				else if FLY_BLOCKING(recvlen)
					goto continuation;
				else
					goto error;
			default:
				break;
			}
		}
		total += recvlen;
		if (fly_update_buffer(__buf, recvlen) == -1)
			return -1;
	}
end_of_connection:
	return FLY_REQUEST_RECEIVE_END;
continuation:
	return FLY_REQUEST_RECEIVE_SUCCESS;
error:
	return FLY_REQUEST_RECEIVE_ERROR;
read_blocking:
	if (total > 0)
		goto continuation;
	return FLY_REQUEST_RECEIVE_READ_BLOCKING;
write_blocking:
	return FLY_REQUEST_RECEIVE_WRITE_BLOCKING;
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
	fly_buffer_c *request_line_buf;
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
	switch (fly_request_receive(event->fd, request->connect)){
	case FLY_REQUEST_RECEIVE_ERROR:
		goto error;
	case FLY_REQUEST_RECEIVE_END:
		/* end of connection */
		goto disconnection;
	case FLY_REQUEST_RECEIVE_SUCCESS:
		break;
	case FLY_REQUEST_RECEIVE_READ_BLOCKING:
		goto read_continuation;
	case FLY_REQUEST_RECEIVE_WRITE_BLOCKING:
		goto write_continuation;
	default:
		FLY_NOT_COME_HERE
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
	request_line_buf = fly_get_request_line_buf(request->buffer);
	if (fly_unlikely_null(request_line_buf))
		goto response_400;
	switch(__fly_request_operation(request, request_line_buf)){
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
		goto read_continuation;
	default:
		break;
	}

	/* parse header */
__fase_header:
	char *header_ptr;

	fly_event_fase(event, HEADER);
	header_ptr = fly_get_header_lines_ptr(request->buffer->chain);
	if (header_ptr == NULL)
		goto read_continuation;

	switch (fly_reqheader_operation(request, fly_buffer_chain_from_ptr(request->buffer, header_ptr), header_ptr)){
	case __REQUEST_HEADER_ERROR:
		goto response_400;
	case __REQUEST_HEADER_IN_THE_MIDDLE:
		goto read_continuation;
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
	body_ptr = fly_get_body_ptr(request->buffer->chain->ptr);
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
	if (fly_400_event(event, request) == -1)
		goto error;
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
	if (fly_414_event(event, request) == -1)
		goto error;
	return 0;
response_415:
	if (fly_415_event(event, request) == -1)
		goto error;
	return 0;
response_500:
	return 0;
response_501:
	return 0;

/* continuation event publish. */
write_continuation:
	event->read_or_write = FLY_WRITE;
	goto continuation;
read_continuation:
	event->read_or_write = FLY_READ;
	goto continuation;
continuation:
	event->event_state = (void *) EFLY_REQUEST_STATE_CONT;
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

	return fly_response_from_pf(event, request, pf);

response_304:
	struct fly_response_content *rc_304;

	rc_304 = fly_pballoc(request->pool, sizeof(struct fly_response_content));
	if (fly_unlikely_null(rc_304))
		goto error;
	rc_304->pf = pf;
	rc_304->request = request;
	event->event_data = (void *) rc_304;
	if (fly_304_event(event) == -1)
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

int fly_create_response_from_request(fly_request_t *req __unused, fly_response_t **res __unused)
{
	return 0;
}

int fly_hv2_request_target_parse(fly_request_t *req)
{
	struct fly_request_line *reqline = req->request_line;
	char *ptr=NULL, *query=NULL;
	size_t len;
	enum{
		INIT,
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
		END,
	} status;
	enum method_type method_type;

	status = INIT;
	method_type = reqline->method->type;
	ptr = reqline->uri.ptr;
	len = reqline->uri.len;
	if (!ptr)
		goto error;

	while(true){
		switch(status){
		case INIT:
			if (__fly_space(*ptr)) break;

			switch (method_type){
			case CONNECT:
				status = AUTHORITY_FORM;
				continue;
			default: break;
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
			else if (__fly_segment(&ptr))	break;
			if (__fly_question(*ptr)){
				status = ORIGIN_FORM_QUESTION;
				break;
			}
			goto error;
		case ORIGIN_FORM_QUESTION:
			query = ptr;
			if (__fly_query(&ptr)){
				status = ORIGIN_FORM_QUERY;
				break;
			}

			goto error;
		case ORIGIN_FORM_QUERY:
			if (__fly_query(&ptr))	break;
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
			query = ptr;
			if (__fly_query(&ptr)){
				status = ABSOLUTE_FORM_QUERY;
				break;
			}

			goto error;
		case ABSOLUTE_FORM_QUERY:
			if (__fly_query(&ptr))	break;
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
			goto error;
		case ASTERISK_FORM:
			break;
		case END:
			if (query)
				__fly_query_set(req, query);
			return 0;
		}

		if (!--len){
			status = END;
			continue;
		}
		ptr++;
	}
error:
	return -1;
}
