#include <stdio.h>
#include <ctype.h>
#include "alloc.h"
#include "request.h"
#include "response.h"
#include "version.h"
#include "util.h"

fly_request_t *fly_request_init(void)
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
	memset(req->buffer, 0, FLY_BUFSIZE);

	return req;
}

int fly_request_release(fly_request_t *req)
{
	if (req->header != NULL)
		fly_delete_pool(req->header->pool);
	return fly_delete_pool(req->pool);
}

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer)
{
	return buffer;
}

__fly_static int __fly_alpha(char c)
{
	if ((c>='a' && c<='z') || (c>='A' && c<='Z'))
		return 1;
	else
		return 0;
}
__fly_static int __fly_number(char c)
{
	if (c>='0' && c<='9')
		return 1;
	else
		return 0;
}
__fly_static int __fly_alpha_number(char c)
{
	return (__fly_alpha(c) || __fly_number(c)) ? 1 : 0;
}
__fly_static int __fly_space(char c)
{
	return (c == ' ') ? 1 : 0;
}
__fly_static int __fly_ht(char c)
{
	return (c == '\t' ? 1 : 0);
}
__fly_static int __fly_slash(char c)
{
	return (c == '/') ? 1 : 0;
}
__fly_static int __fly_dot(char c)
{
	return (c == '.') ? 1 : 0;
}
__fly_static int __fly_cr(char c)
{
	return (c == '\r') ? 1 : 0;
}
__fly_static int __fly_lf(char c)
{
	return (c == '\n') ? 1 : 0;
}
__fly_static int __fly_colon(char c)
{
	return (c == ':') ? 1 : 0;
}
__unused __fly_static int __fly_bracket(char c)
{
	return (c == '[' || c == ']') ? 1 : 0;
}
__unused __fly_static int __fly_gtlt(char c)
{
	return (c == '<' || c == '>') ? 1 : 0;
}
__unused __fly_static int __fly_equal(char c)
{
	return (c == '=') ? 1 : 0;
}
__fly_static int __fly_vchar(char c)
{
	return (c >= 0x21 && c <= 0x7E) ? 1 : 0;
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
    printf("%s\n", request_line);
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
    printf("%s\n", req->uri.uri);

    char *version_start = next_space+1;
    req->version = fly_match_version(version_start);
	/* unmatch version */
    if (req->version == NULL)
		return FLY_ERROR(400);

    printf("%s\n", req->version->full);
    char *slash_p = strchr(req->version->full,'/');
	/* http version number not found */ 
    if (slash_p == NULL)
		return FLY_ERROR(400);

    req->version->number = slash_p + 1;
    printf("%s\n", req->version->number);
    return 0;
}

int fly_request_operation(int c_sock, fly_pool_t *pool,fly_reqlinec_t *request_line, fly_request_t *req)
{
    /* get request */
    int request_line_length;
	/* request line parse check */
	if (__fly_parse_reqline(request_line) == -1)
		goto error_400;

	if (strstr(request_line, "\r\n") == NULL)
		goto error_not_found_request_line;

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
    printf("Request Line: %s\n", req->request_line->request_line);
    printf("Request Line LENGTH: %ld\n", strlen(req->request_line->request_line));

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
	fly_400_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
	return -1;
error_414:
	/* URI Too Long */
	fly_414_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
	return -1;
error_not_found_request_line:
	fly_notfound_request_line(c_sock, FLY_DEFAULT_HTTP_VERSION);
	return -1;
error_500:
	/* Server Error */
	fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
	return -1;
error_501:
	/* Request line Too long */
	fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
	return -1;
	
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
	else if (__fly_cr(*ptr) + __fly_lf(*(ptr+1))){
		return 1;
	}else
		return 0;
}

enum __fly_parse_type_result_type{
	SUCCESS,
	ERROR,
	FATAL,
	END_OF_HEADER
};
struct __fly_parse_header_line_result{
	fly_buffer_t *ptr;
	enum __fly_parse_type_result_type type;
};

__fly_static int __fly_parse_header_line(fly_buffer_t *header, struct __fly_parse_header_line_result *res, char *name, char *value)
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
	while(1){
		switch(now){
		case INIT:
			if (!__fly_header_name_usable(*ptr)){
				if (__fly_end_of_header(ptr))
					goto end_of_header;
				else
					goto error;
			}

			now = NAME;
			prev = INIT;
			continue;
		case NAME:
			if (__fly_header_name_usable(*ptr))
				;
			else if (__fly_colon(*ptr)){
				now = GAP;
				prev = NAME;
				/* end of name */
				*name = '\0';
				continue;
			}else
				goto error;
			
			/* copy name */
			*name++ = *ptr;
			prev = NAME;
			break;
		case GAP:
			if (__fly_colon(*ptr))
				;
			else if (__fly_header_gap_usable(*ptr))
				now = GAP_SPACE;
			else if (__fly_alpha_number(*ptr)){
				now = VALUE;
				prev = GAP;
				continue;
			}else if (__fly_cr(*ptr))
				now = CR;
			else if (__fly_lf(*ptr))
				now = LF;
			else
				goto error;

			prev = GAP;
			break;
		case GAP_SPACE:
			if (__fly_header_gap_usable(*ptr))
				;
			else if (__fly_alpha_number(*ptr)){
				now = VALUE;
				prev = GAP_SPACE;
				continue;
			}else if (__fly_cr(*ptr))
				now = CR;
			else if (__fly_lf(*ptr))
				now = LF;
			else
				goto error;
			prev = GAP_SPACE;
			break;
		case VALUE:
			if (__fly_header_value_usable(*ptr))
				;
			else if (__fly_cr(*ptr)){
				now = CR;
				*value = '\0';
				prev = VALUE;
				continue;
			}else if (__fly_lf(*ptr)){
				now = LF;
				*value = '\0';
				prev = VALUE;
				continue;
			}else
				goto error;
			
			*value++ = *ptr;
			prev = VALUE;
			break;
		case CR:
			if (prev == VALUE && __fly_lf(*ptr))
				now = LF;
			else
				goto error;

			prev = CR;
			break;
		case LF:
			now = NEXT;

			prev = LF;
			continue;
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
	res->type = SUCCESS;
	return 0;
end_of_header:
	res->ptr = ptr;
	res->type = END_OF_HEADER;
	return 0;
error:
	if (strchr(ptr, FLY_LF) == NULL){
		res->ptr = ptr;
		res->type = FATAL;
		return -1;
	}else{
		while( !__fly_lf(*ptr++) )
			;
		res->ptr = ptr;
		res->type = ERROR;
		return -1;
	}
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
				char name[FLY_HEADER_NAME_MAX];
				char value[FLY_HEADER_VALUE_MAX];
				struct __fly_parse_header_line_result result;
				__fly_parse_header_line(ptr, &result, name, value);
				switch(result.type){
				case END_OF_HEADER:
					now = END;
					continue;
				case FATAL:
					goto fatal;
				case ERROR:
					break;
				case SUCCESS:
					if (fly_header_add(ci, name, value) == -1)
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

error:
	return -1;
fatal:
	return -1;
end:
	return 0;
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

