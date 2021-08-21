#ifndef _REQUEST_H
#define _REQUEST_H

#include <string.h>
#include "alloc.h"
#include "method.h"
#include "version.h"

typedef struct{
	http_method *method;
	char *uri;
	http_version_t *version;
} request_info;

typedef struct{
	char *request_line;
	char **header_lines;
	int header_len;
	char *body;
	request_info *rinfo;
} http_request;

int fly_request_operation(int c_sock, fly_pool_t *pool,char *request_line, http_request *req);

#endif
