#include <stdio.h>
#include <ctype.h>
#include "alloc.h"
#include "request.h"
#include "response.h"
#include "version.h"
#include "util.h"

fly_reqlinec_t *fly_get_request_line_ptr(char *buffer)
{
	return buffer;
}

static int parse_request_line(fly_pool_t *pool, __unused int c_sock, fly_reqline_t *req)
{
	fly_reqlinec_t *request_line;
    char *method;
    char *space;

	request_line = req->request_line;
    printf("%s\n", request_line);
    space = strstr(request_line, " ");
    /* method only => response 400 Bad Request */
    if (space == NULL){
        fly_notfound_uri(c_sock, FLY_DEFAULT_HTTP_VERSION);
        return -1;
    }
    method = fly_pballoc(pool, space-request_line+1);
	/* memory alloc error */
	if (method == NULL){
		return -1;
	}
    memcpy(method, request_line, space-request_line);
    method[space-request_line] = '\0';
    req->method = fly_match_method_name(method);
	/* no match method */
    if (req->method == NULL){
		fly_unmatch_request_method(c_sock, FLY_DEFAULT_HTTP_VERSION);
        return -1;
	}

    char *uri_start = space+1;
    char *next_space = strstr(uri_start," ");
	if (next_space == NULL){
		fly_notfound_http_version(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    req->uri.uri = fly_pballoc(pool, next_space-uri_start+1);
	if (req->uri.uri == NULL){
		fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    memcpy(req->uri.uri, space+1, next_space-uri_start);
    req->uri.uri[next_space-uri_start] = '\0';
    printf("%s\n", req->uri.uri);

    char *version_start = next_space+1;
    req->version = fly_match_version(version_start);
    if (req->version == NULL){
		/* no version */
		fly_unmatch_http_version(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
    }

    printf("%s\n", req->version->full);
    char *slash_p = strchr(req->version->full,'/');
	/* http version number not found */ 
    if (slash_p == NULL){
		fly_nonumber_http_version(c_sock, FLY_DEFAULT_HTTP_VERSION);
        return -1;
	}

    req->version->number = slash_p + 1;
    printf("%s\n", req->version->number);
    return 0;
}

int fly_request_operation(int c_sock, fly_pool_t *pool,fly_reqlinec_t *request_line, fly_request_t *req)
{
    /* get request */
    int request_line_length;
	if (strstr(request_line, "\r\n") == NULL){
		fly_notfound_request_line(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    request_line_length = strstr(request_line, "\r\n") - request_line;
	req->request_line = fly_pballoc(pool, sizeof(fly_reqline_t));
    req->request_line->request_line = fly_pballoc(pool, sizeof(fly_reqlinec_t)*(request_line_length+1));
	if (req->request_line == NULL){
		fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
	if (req->request_line->request_line == NULL){
		fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    memcpy(req->request_line->request_line, request_line, request_line_length);
    /* get total line */
    req->request_line->request_line[request_line_length] = '\0';
    printf("Request Line: %s\n", req->request_line->request_line);
    printf("Request Line LENGTH: %ld\n", strlen(req->request_line->request_line));
    if (parse_request_line(pool, c_sock, req->request_line) < 0){
        return -1;
    }
    return 0;
}



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
	req->headers = NULL;
	req->body = NULL;
	req->buffer = fly_pballoc(pool, FLY_BUFSIZE);
	if (req->buffer == NULL)
		return NULL;
	memset(req->buffer, 0, FLY_BUFSIZE);

	return req;
}

int fly_request_release(fly_request_t *req)
{
	fly_delete_pool(req->headers->pool);
	return fly_delete_pool(req->pool);
}

int fly_reqheader_operation(fly_request_t *req, fly_buffer_t *headers)
{
	if (headers == NULL)
		return 0;

	fly_pool_t *rh_pool;
	fly_hdr_ci *rchain_info;
	fly_buffer_t *header_line, *header_line_end;

	rh_pool = fly_create_pool(FLY_REQHEADER_POOL_SIZE);
	if (rh_pool == NULL)
		return -1;
	rchain_info = fly_header_init();
	if (rchain_info == NULL)
		return -1;
	
	header_line = headers;
    while (1){
		char name[FLY_HEADER_NAME_MAX];
		char value[FLY_HEADER_LINE_MAX-FLY_HEADER_NAME_MAX];

        header_line_end = strstr(header_line, "\r\n");
        if (header_line_end == NULL || header_line_end-header_line == 0)
            break;

		fly_until_strcpy(name, header_line, " :", header_line_end);
		fly_until_strcpy(value, header_line+strlen(name), "\r\n", header_line_end);
		value[strlen(value)] = '\0';
		if (fly_header_add(rchain_info, name, value) == -1)
			return -1;

        /* next line */
        header_line = header_line_end + CRLF_LENGTH;
    }
    req->headers = rchain_info;

//    for (int j=0; j<req->header_len; j++){
//        printf("HEADER_Line: %s\n", header_lines[j]);
//    }
	return 0;
}

