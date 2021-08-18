#ifndef _RESPONSE_H
#define _RESPONSE_H
#include "header.h"

#define RESPONSE_LENGTH_PER		1024
#define DEFAULT_RESPONSE_VERSION			"1.1"
#define CRLF_LENGTH				2

typedef struct{
	char *response_line;
	char **header_lines;
	int header_len;
	char *body;
	int body_len;
} http_response;

int fly_response(
	int c_sockfd,
	int response_code,
	char *version,
	struct fly_hdr_elem *header_lines,
	int header_len,
	char *body,
	int body_len
);

enum response_code_type{
	/* Client Error 4xx */
	_400,
	_401,
	_402,
	_403,
	_404,
	_405,
	_406,
	_407,
	_408,
	_409,
	_410,
	_411,
	_412,
	_413,
	_414,
	_415,
	_416,
	_417,
	/* Server Error 5xx */
	_500
};

typedef struct{
	int status_code;
	enum response_code_type type;
	char *explain;
} response_code;

void response_free(char *);
char *response_raw(http_response *res, int *send_len);
int response_code_from_type(enum response_code_type type);

#endif
