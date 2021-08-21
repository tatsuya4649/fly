#include <stdio.h>
#include <ctype.h>
#include "request.h"
#include "response.h"
#include "version.h"
#include "util.h"


static int parse_request_line(fly_pool_t *pool, __unused int c_sock, char *request_line, request_info *req)
{
    char *method;
    char *space;
    printf("%s\n", request_line);
    space = strstr(request_line, " ");
    /* method only => response 400 Bad Request */
    if (space == NULL){
        fly_notfound_uri(c_sock, FLY_DEFAULT_HTTP_VERSION);
        return -1;
    }
    method = fly_palloc(pool, fly_page_convert((space-request_line)+1));
	/* memory alloc error */
	if (method == NULL){
		return -1;
	}
    memcpy(method, request_line, space-request_line);
    method[space-request_line] = '\0';
    req->method = fly_match_method(method);
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
    req->uri = fly_palloc(pool, fly_page_convert(next_space-uri_start+1));
	if (req->uri == NULL){
		fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    memcpy(req->uri, space+1, next_space-uri_start);
    req->uri[next_space-uri_start] = '\0';
    printf("%s\n", req->uri);

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

int fly_request_operation(int c_sock, fly_pool_t *pool,char *request_line, http_request *req)
{
    /* get request */
    int request_line_length;
	if (strstr(request_line, "\r\n") == NULL){
		fly_notfound_request_line(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    request_line_length = strstr(request_line, "\r\n") - request_line;
    req->request_line = (char *) fly_palloc(pool, sizeof(char)*(request_line_length+1));
	if (req->request_line == NULL){
		fly_500_error(c_sock, FLY_DEFAULT_HTTP_VERSION);
		return -1;
	}
    memcpy(req->request_line, request_line, request_line_length);
    /* get total line */
    req->request_line[request_line_length] = '\0';
    printf("Request Line: %s\n", req->request_line);
    printf("Request Line LENGTH: %ld\n", strlen(req->request_line));
    if (parse_request_line(
		pool,
        c_sock,
        req->request_line,
        req->rinfo
    ) < 0){
        return -1;
    }
    return 0;
}

