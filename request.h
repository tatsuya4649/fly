#ifndef _REQUEST_H
#define _REQUEST_H

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

#endif
