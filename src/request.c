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
#include "char.h"

int fly_request_disconnect_handler(fly_event_t *event);
int fly_request_timeout_handler(fly_event_t *event);
__fly_static int __fly_request_operation(fly_request_t *req, fly_buffer_c *request_line);

fly_request_t *fly_request_init(fly_connect_t *conn)
{
	fly_pool_t *pool;
	fly_request_t *req;
	pool = fly_create_pool(FLY_POOL_MANAGER_FROM_EVENT(conn->event), FLY_REQUEST_POOL_SIZE);
	if (fly_unlikely_null(pool))
		return NULL;
	req = fly_pballoc(pool, sizeof(fly_request_t));
	if (fly_unlikely_null(req))
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
	req->connect = conn;			/* use connect pool */
	req->fase = EFLY_REQUEST_FASE_REQUEST_LINE;
	req->ctx = conn->event->manager->ctx;

	return req;
}

void fly_request_release(fly_request_t *req)
{
#ifdef DEBUG
	assert(req != NULL);
#endif

	if (req->header)
		fly_header_release(req->header);

	if (req->body)
		fly_body_release(req->body);

	if (req->request_line)
		fly_request_line_release(req);

	fly_delete_pool(req->pool);
}

int fly_request_line_init(fly_request_t *req)
{
	req->request_line = fly_pballoc(req->pool, sizeof(struct fly_request_line));
	if (fly_unlikely_null(req->request_line))
		return -1;
	req->request_line->request_line = NULL;
	req->request_line->method = NULL;
	req->request_line->uri.ptr = NULL;
	req->request_line->uri.len = 0;
	req->request_line->version = NULL;
	fly_request_query_init(&req->request_line->query);

	if (req->connect->flag & FLY_SSL_CONNECT)
		req->request_line->scheme = fly_match_scheme_type(fly_https);
	else
		req->request_line->scheme = fly_match_scheme_type(fly_http);

	if (fly_unlikely_null(req->request_line->scheme))
		return -1;
	return 0;
}

void fly_request_line_release(fly_request_t *req)
{
	fly_pbfree(req->pool, req->request_line);
}

struct fly_buffer_chain *fly_get_request_line_buf(fly_buffer_t *__buf)
{
	return fly_buffer_first_chain(__buf);
}

static inline bool __fly_vchar(char c)
{
	return (c >= 0x21 && c <= 0x7E) ? true : false;
}
static inline bool __fly_zero(char c)
{
	return (c == '\0') ? true : false;
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
		fly_numeral(c) || fly_alpha(c) || (c!=';' && __fly_vchar(c)) \
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
	return (fly_alpha(c) || fly_numeral(c) || \
		c=='=' || c=='.' || c=='_' || c==0x7E
	) ? true : false;
}

static inline bool __fly_pct_encoded(char **c, ssize_t *len)
{
	if (**c != '%')
		return false;
	if (!__fly_hexdigit(*(*c+1)))
		return false;
	if (!__fly_hexdigit(*(*c+2)))
		return false;

	*c += 2;
	if (len != NULL)
		len -= 2;
	return true;
}

static inline bool __fly_pchar(char **c, ssize_t *len)
{
	return (__fly_unreserved(**c) || fly_colon(**c) ||	\
		__fly_sub_delims(**c) || fly_atsign(**c) ||		\
		__fly_pct_encoded(c, len)								\
	) ? true : false;
}

static inline bool __fly_segment(char **c, ssize_t *len)
{
	return __fly_pchar(c, len);
}

static inline bool __fly_query(char **c, ssize_t *len)
{
	return (								\
		__fly_pchar(c, len) || fly_slash(**c) || fly_question(**c) \
	) ? true : false;
}

static bool __fly_hier_part(char **c)
{
	if (!(fly_slash(**c) && fly_slash(*(*c+1))))
		return false;
	return true;
}

static inline bool __fly_userinfo(char **c, ssize_t *len)
{
	return (__fly_unreserved(**c) || __fly_sub_delims(**c) || \
		fly_colon(**c) || __fly_pct_encoded(c, len)			  \
	) ? true : false;
}

static inline bool __fly_port(char c)
{
	return fly_numeral(c);
}

static inline bool __fly_host(char **c)
{
	return fly_alpha_numeral(**c);
}

static bool __fly_http(char **c, ssize_t *len)
{
	/* HTTP */
	if (!(**c == 0x48 && *(*c+1) == 0x54 && *(*c+2) == 0x54 && *(*c+3) == 0x50))
		return false;
	*c += 3;
	if (len != NULL)
		*len -= 3;
	return true;
}

static inline void __fly_query_set(fly_request_t *req, fly_reqlinec_t *c, size_t len)
{
	req->request_line->query.ptr = c;
	req->request_line->query.len = len;
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
	const char *ptr;
	ptr = __m->name;
	while(*ptr)
		if (*ptr++ != *c++)
			return -1;

	return __m->type;
}

__fly_static int __fly_parse_reqline(fly_request_t *req, fly_reqlinec_t *request_line, ssize_t len)
{
	fly_reqlinec_t *ptr=NULL, *method=NULL, *http_version=NULL, *request_target=NULL, *query=NULL;
	__unused size_t method_len, version_len, target_len, query_len;
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

	method_len = 0;
	version_len = 0;
	target_len = 0;
	query_len = 0;
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
				else if (fly_space(*ptr)){
					/* request method */
					method_len = ptr-method;
					req->request_line->method = fly_match_method_name_len(method, method_len);
					if (req->request_line->method == NULL)
						goto error;
					status = METHOD_SPACE;
					continue;
				}

				goto error;
			case METHOD_SPACE:
				if (fly_space(*ptr)) break;

				request_target = ptr;
				method_type = __fly_request_method(method);
				switch (method_type){
				case CONNECT:
					status = AUTHORITY_FORM;
					continue;
				default: break;;
				}

				if (fly_asterisk(*ptr) && method_type==OPTIONS){
					status = ASTERISK_FORM;
					continue;
				}else if (fly_slash(*ptr)){
					status = ORIGIN_FORM;
					continue;
				}else{
					status = ABSOLUTE_FORM;
					continue;
				}

				goto error;
			case ORIGIN_FORM:
				if (fly_slash(*ptr))	break;
				else if (__fly_segment(&ptr, &len))	break;
				if (fly_space(*ptr)){
					target_len = ptr-request_target;
					status = END_REQUEST_TARGET;
					continue;
				}
				if (fly_question(*ptr)){
					status = ORIGIN_FORM_QUESTION;
					break;
				}
				goto error;
			case ORIGIN_FORM_QUESTION:
				if (fly_space(*ptr)){
					target_len = ptr-request_target;
					status = END_REQUEST_TARGET;
					continue;
				}

				if (__fly_query(&ptr, &len)){
					query = ptr;
					status = ORIGIN_FORM_QUERY;
					break;
				}

				goto error;
			case ORIGIN_FORM_QUERY:
				if (__fly_query(&ptr, &len))	break;
				if (fly_space(*ptr)){
					query_len = ptr-query;
					target_len = ptr-request_target;
					status = END_REQUEST_TARGET;
					continue;
				}
				goto error;

			case ABSOLUTE_FORM:
				if (!is_fly_scheme(&ptr, ':'))
					goto error;
				if (fly_colon(*ptr)){
					status = ABSOLUTE_FORM_COLON;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_COLON:
				if (!__fly_hier_part(&ptr))
					goto error;
				if (fly_question(*ptr)){
					status = ABSOLUTE_FORM_QUESTION;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_QUESTION:
				if (fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}

				if (__fly_query(&ptr, &len)){
					query = ptr;
					status = ABSOLUTE_FORM_QUERY;
					break;
				}

				goto error;
			case ABSOLUTE_FORM_QUERY:
				if (__fly_query(&ptr, &len))	break;
				if (fly_space(*ptr)){
					query_len = ptr-query;
					target_len = ptr-request_target;
					status = END_REQUEST_TARGET;
					continue;
				}
				goto error;
			case AUTHORITY_FORM:
				if (__fly_userinfo(&ptr, &len)){
					status = AUTHORITY_FORM_USERINFO;
					break;
				}
				if (__fly_host(&ptr)){
					status = AUTHORITY_FORM_HOST;
					break;
				}
				goto error;
			case AUTHORITY_FORM_USERINFO:
				if (__fly_userinfo(&ptr, &len))	break;

				if (fly_atsign(*ptr)){
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
				if (fly_colon(*ptr)){
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
				if (fly_space(*ptr)){
					status = END_REQUEST_TARGET;
					continue;
				}

				goto error;
			case END_REQUEST_TARGET:
				/* add request/request_line/uri */
				fly_uri_set(req, request_target, target_len);
				if (query)
					__fly_query_set(req, query, query_len);
				status = REQUEST_TARGET_SPACE;
				continue;
			case REQUEST_TARGET_SPACE:
				if (fly_space(*ptr))	break;
				status = HTTP_NAME;
				continue;
			case HTTP_NAME:
				if (__fly_http(&ptr, &len))
					break;
				if (fly_slash(*ptr)){
					status = HTTP_SLASH;
					break;
				}
				goto error;
			case HTTP_SLASH:
				if (fly_numeral(*ptr)){
					http_version = ptr;
					status = HTTP_VERSION_MAJOR;
					break;
				}
				goto error;
			case HTTP_VERSION_MAJOR:
				if (fly_dot(*ptr)){
					status = HTTP_VERSION_POINT;
					break;
				}
				if (fly_cr(*ptr)){
					version_len = ptr-http_version;
					status = END_HTTP_VERSION;
					break;
				}
				goto error;
			case HTTP_VERSION_POINT:
				if (fly_numeral(*ptr)){
					status = HTTP_VERSION_MINOR;
					break;
				}
				goto error;
			case HTTP_VERSION_MINOR:
				if (fly_cr(*ptr)){
					version_len = ptr-http_version;
					status = END_HTTP_VERSION;
					break;
				}
				goto error;
			case END_HTTP_VERSION:
				/* add http version */
				req->request_line->version = fly_match_version_len(http_version, version_len);
				if (!req->request_line->version)
					goto error;
				status = CR;
				continue;
			case CR:
				if (fly_lf(*ptr)){
					status = LF;
					break;
				}
				goto error;
			case LF:
				status = SUCCESS;
				continue;
			case SUCCESS:
				return 1;
			default:
				goto error;
				break;
		}
		ptr++;
		if (--len <= 0 && status != LF)
			goto error;
	}
error:
	return -1;
}

__fly_static int __fly_request_operation(fly_request_t *req, fly_buffer_c *reqline_bufc)
{
	/* get request */
	size_t request_line_length;
	fly_buf_p rptr;

	/* not ready for request line */
	if ((rptr=fly_buffer_strstr_after(reqline_bufc, "\r\n")) == NULL)
		goto not_ready;

	request_line_length = fly_buffer_ptr_len(reqline_bufc->buffer, rptr, reqline_bufc->use_ptr);
	if (request_line_length >= FLY_REQUEST_LINE_MAX)
		goto error_414;

	if (fly_request_line_init(req) == -1)
		return -1;
	req->request_line->request_line = fly_pballoc(req->pool, sizeof(fly_reqlinec_t)*(request_line_length+1));
	req->request_line->request_line_len = request_line_length;

	if (fly_unlikely_null(req->request_line))
		goto error_500;
	if (fly_unlikely_null(req->request_line->request_line))
		goto error_500;

	fly_buffer_memcpy(req->request_line->request_line, reqline_bufc->use_ptr, reqline_bufc, request_line_length);

	/* get total line */
	req->request_line->request_line[request_line_length] = '\0';

	/* request line parse check */
	if (__fly_parse_reqline(req, req->request_line->request_line, request_line_length) == -1)
		goto error_400;

	fly_buffer_chain_release_from_length(reqline_bufc, request_line_length);
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
	return (fly_space(c) || fly_ht(c)) ? true : false;
}

static inline bool __fly_field_content(char c)
{
	return (__fly_vchar(c) || fly_space(c) || fly_ht(c)) \
		? true : false;
}

static inline bool __fly_header_field_value(char c)
{
	return __fly_field_content(c);
}
__fly_static bool __fly_end_of_header(char *ptr)
{
	if (fly_lf(*ptr))
		return true;
	else if (fly_cr(*ptr)){
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
			if (fly_colon(*ptr)){
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
			if (fly_cr(*ptr)){
				status = CR;
				break;
			}
			if (fly_lf(*ptr)){
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
			if (fly_cr(*ptr)){
				status = CR;
				break;
			}
			if (fly_lf(*ptr)){
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
			if (fly_lf(*ptr)){
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
		while( !fly_lf(*ptr++) )
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

__fly_static int __fly_parse_header(fly_hdr_ci *ci, fly_buffer_c *header_chain)
{
	enum {
		HEADER_LINE,
		END
	} status;

	fly_buffer_t *__buf = header_chain->buffer;
	fly_buffer_c *chain = header_chain;
	fly_buf_p ptr = header_chain->use_ptr;
	if (fly_unlikely_null(header_chain))
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
				ssize_t len;

				len = fly_buffer_ptr_len(__c->buffer, result.ptr, ptr);
				fly_buffer_chain_release_from_length(__c, len);
				ptr = result.ptr;
				chain = fly_buffer_first_chain(__buf);
			}
			break;
		case END:
			fly_buffer_chain_release_from_length(chain, FLY_CRLF_LENGTH);
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

int fly_reqheader_operation(fly_request_t *req, fly_buffer_c *header_chain)
{
	fly_hdr_ci *rchain_info;
	rchain_info = fly_header_init(req->ctx);
	if (fly_unlikely_null(rchain_info))
		return -1;

	req->header = rchain_info;
	return __fly_parse_header(rchain_info, header_chain);
}

int fly_request_receive(fly_sock_t fd, fly_connect_t *connect)
{
#ifdef DEBUG
	assert(connect != NULL);
	assert(connect->buffer != NULL);
#endif

	fly_buffer_t *__buf;

	__buf = connect->buffer;
	if (fly_unlikely(__buf->chain_count == 0))
		return -1;

	int recvlen=0, total=0;
	while(true){
		if (FLY_CONNECT_ON_SSL(connect)){
			SSL *ssl = connect->ssl;

			recvlen = SSL_read(ssl, fly_buffer_lunuse_ptr(__buf),  fly_buffer_lunuse_len(__buf));
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
				if (errno == EPIPE || errno == 0)
					goto end_of_connection;
				goto error;
			case SSL_ERROR_SSL:
				goto error;
			default:
				/* unknown error */
				goto error;
			}
		}else{
			recvlen = recv(fd, fly_buffer_lunuse_ptr(__buf), fly_buffer_lunuse_len(__buf), MSG_DONTWAIT);
			switch(recvlen){
			case 0:
				goto end_of_connection;
			case -1:
				if (errno == EINTR)
					continue;
				else if FLY_BLOCKING(recvlen)
					goto read_blocking;
				else if (errno == ECONNREFUSED)
					goto end_of_connection;
				else
					goto error;
			default:
				break;
			}
		}
		total += recvlen;
		switch(fly_update_buffer(__buf, recvlen)){
		case FLY_BUF_ADD_CHAIN_SUCCESS:
			break;
		case FLY_BUF_ADD_CHAIN_LIMIT:
			goto overflow;
		case FLY_BUF_ADD_CHAIN_ERROR:
			goto buffer_error;
		default:
			FLY_NOT_COME_HERE
		}
	}
end_of_connection:
	connect->peer_closed = true;
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
buffer_error:
	return FLY_REQUEST_RECEIVE_ERROR;
overflow:
	return FLY_REQUEST_RECEIVE_OVERFLOW;
}

int fly_request_disconnect_handler(fly_event_t *event)
{
	__unused fly_request_t *req;

	event->flag |= FLY_CLOSE_EV;

	req = (fly_request_t *) event->event_data;

	fly_connect_release(req->connect);
	fly_request_release(req);

	/* release some resources */
	if (fly_event_unregister(event) == -1)
		return -1;
	return 0;
}

int fly_request_timeout_handler(fly_event_t *event)
{
	fly_request_t *req;
	fly_connect_t *conn;
	req = (fly_request_t *) event->expired_event_data;

	conn = req->connect;

	/* release some resources */
	/* close socket and release resources. */
	fly_request_release(req);

	event->flag = FLY_CLOSE_EV;
	if (fly_connect_release(conn) == -1)
		return -1;
	return 0;
}

#include "cache.h"
int fly_request_event_handler(fly_event_t *event)
{
	fly_request_t					*request;
	fly_response_t					*response;
	fly_buffer_c					*reline_buf_chain;
	fly_body_t						*body;
	fly_route_reg_t					*route_reg;
	fly_route_t						*route;
	fly_mount_t						*mount;
	struct fly_mount_parts_file		*pf;
	__unused fly_request_state_t	state;
	fly_request_fase_t				fase;
	fly_connect_t					*conn;

	state = (fly_request_state_t) event->event_state;
	fase = (fly_request_fase_t) event->event_fase;
	request = (fly_request_t *) event->event_data;
	conn = request->connect;

	fly_event_fase(event, REQUEST_LINE);
	fly_event_state(event, RECEIVE);
	switch (fly_request_receive(event->fd, conn)){
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
	case FLY_REQUEST_RECEIVE_OVERFLOW:
		goto response_413;
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
	reline_buf_chain = fly_get_request_line_buf(conn->buffer);
	if (fly_unlikely_null(reline_buf_chain))
		goto response_400;
	switch(__fly_request_operation(request, reline_buf_chain)){
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
	fly_buffer_c *hdr_buf;

	fly_event_fase(event, HEADER);
	hdr_buf = fly_get_header_lines_buf(conn->buffer);
	if (hdr_buf == NULL)
		goto read_continuation;

	switch (fly_reqheader_operation(request, hdr_buf)){
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
	if (fly_content_length(request->header) == 0)
		goto __fase_end_of_parse;

	/* parse body */
__fase_body:
	size_t content_length;
	content_length = fly_content_length(request->header);
	if (!content_length)
		goto __fase_end_of_parse;

	fly_buffer_c *body_buf;
	fly_event_fase(event, BODY);
	body = fly_body_init(request->ctx);
	if (body == NULL)
		goto error;
	request->body = body;
	body_buf = fly_get_body_buf(conn->buffer);
	if (body_buf == NULL || conn->buffer->use_len < content_length)
		goto read_continuation;

	/* content-encoding */
	fly_hdr_value *ev;
	fly_encoding_type_t *et;
	ev = fly_content_encoding(request->header);
	if (ev){
		/* not supported encoding */
		et = fly_supported_content_encoding(ev);
		if (!et)
			goto response_415;
		if (fly_decode_body(body_buf, et, body, content_length) == NULL)
			goto error;
	}else{
		char *body_ptr = fly_pballoc(body->pool, sizeof(uint8_t)*content_length);
		if (fly_unlikely_null(body_ptr))
			goto error;

		fly_buffer_memcpy(body_ptr, body_buf->use_ptr, body_buf, content_length);
		fly_body_setting(body, body_ptr, content_length);
	}

	fly_buffer_chain_release_from_length(body_buf, content_length);

	if (fly_is_multipart_form_data(request->header))
		fly_body_parse_multipart(request);

__fase_end_of_parse:
	fly_event_fase(event, RESPONSE);
	/* Success parse request */
	enum method_type __mtype;
	__mtype = request->request_line->method->type;
	route_reg = event->manager->ctx->route_reg;
	/* search from registerd route uri */
	route = fly_found_route(route_reg, &request->request_line->uri, __mtype);
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
	response = route->function(request, route->data);
	if (response == NULL)
		goto response_500;
	fly_response_header_init(response, request);

	if (fly_add_date(response->header, false) == -1)
		goto response_500;
	if (fly_add_server(response->header, false) == -1)
		goto response_500;
	if (fly_add_content_type(response->header, &default_route_response_mime, false) == -1)
		goto response_500;

	response->request = request;
	fly_response_http_version_from_request(response, request);
	goto response;
/* TODO: error response event memory release */
response_400:
	return fly_400_event(event, request);
response_404:
	return fly_404_event(event, request);
response_405:
	return fly_405_event(event, request);
response_413:
	if (request->request_line == NULL && \
			fly_request_line_init(request) == -1)
		goto response_500;
	request->request_line->version = fly_default_http_version();
	return fly_413_event(event, request);
response_414:
	return fly_414_event(event, request);
response_415:
	return fly_415_event(event, request);
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
	return fly_request_disconnect_handler(event);

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
	fly_response_timeout_end_setting(event, response);

	if (fly_event_register(event) == -1)
		goto error;

	return  0;

error:
	return -1;
}


int fly_create_response_from_request(fly_request_t *req __unused, fly_response_t **res __unused)
{
	return 0;
}

int fly_hv2_request_target_parse(fly_request_t *req)
{
	struct fly_request_line *reqline = req->request_line;
	char *ptr=NULL, *query=NULL;
	size_t query_len;
	ssize_t len;
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

	query_len = 0;
	while(true){
		switch(status){
		case INIT:
			if (fly_space(*ptr)) break;

			switch (method_type){
			case CONNECT:
				status = AUTHORITY_FORM;
				continue;
			default: break;
			}

			if (fly_asterisk(*ptr) && method_type==OPTIONS){
				status = ASTERISK_FORM;
				continue;
			}else if (fly_slash(*ptr)){
				status = ORIGIN_FORM;
				continue;
			}else{
				status = ABSOLUTE_FORM;
				continue;
			}
			goto error;
		case ORIGIN_FORM:
			if (fly_slash(*ptr))	break;
			else if (__fly_segment(&ptr, &len))	break;
			if (fly_question(*ptr)){
				query = ptr;
				status = ORIGIN_FORM_QUESTION;
				break;
			}
			goto error;
		case ORIGIN_FORM_QUESTION:
			if (__fly_query(&ptr, &len)){
				status = ORIGIN_FORM_QUERY;
				break;
			}

			goto error;
		case ORIGIN_FORM_QUERY:
			if (__fly_query(&ptr, &len))	break;
			goto error;

		case ABSOLUTE_FORM:
			if (!is_fly_scheme(&ptr, ':'))
				goto error;
			if (fly_colon(*ptr)){
				status = ABSOLUTE_FORM_COLON;
				break;
			}

			goto error;
		case ABSOLUTE_FORM_COLON:
			if (!__fly_hier_part(&ptr))
				goto error;
			if (fly_question(*ptr)){
				query = ptr;
				status = ABSOLUTE_FORM_QUESTION;
				break;
			}

			goto error;
		case ABSOLUTE_FORM_QUESTION:
			if (__fly_query(&ptr, &len)){
				status = ABSOLUTE_FORM_QUERY;
				break;
			}

			goto error;
		case ABSOLUTE_FORM_QUERY:
			if (__fly_query(&ptr, &len))	break;
			goto error;
		case AUTHORITY_FORM:
			if (__fly_userinfo(&ptr, &len)){
				status = AUTHORITY_FORM_USERINFO;
				break;
			}
			if (__fly_host(&ptr)){
				status = AUTHORITY_FORM_HOST;
				break;
			}
			goto error;
		case AUTHORITY_FORM_USERINFO:
			if (__fly_userinfo(&ptr, &len))	break;

			if (fly_atsign(*ptr)){
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
			if (fly_colon(*ptr)){
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
				__fly_query_set(req, query, query_len);
			return 0;
		}

		if (!--len){
			status = END;
			continue;
		}
		ptr++;
		if (--len == 0){
			if (query)
				query_len = ptr-query;
			status = END;
		}
	}
error:
	return -1;
}

int fly_request_timeout(void)
{
	return fly_config_value_int(FLY_REQUEST_TIMEOUT);
}
