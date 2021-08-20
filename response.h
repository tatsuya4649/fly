#ifndef _RESPONSE_H
#define _RESPONSE_H
#include "header.h"

#define RESPONSE_LENGTH_PER		1024
#define FLY_RESPONSE_POOL_PAGE		100
#define DEFAULT_RESPONSE_VERSION			"1.1"
#define CRLF_LENGTH				2

typedef struct{
	char *status_line;
	char **header_lines;
	int header_lines_len;
	char *body;
	ssize_t body_len;
} http_response;

int fly_response(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	char *version,
	fly_hdr_t *header_lines,
	int header_len,
	char *body,
	ssize_t body_len
);
int fly_response_file(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	char *version,
	fly_hdr_t *header_lines,
	int header_len,
	char *file_path,
	int mount_number,
	fly_pool_s size
);
fly_pool_t *fly_response_init(void);
int fly_response_release(fly_pool_t *respool);

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
typedef enum response_code_type fly_rescode_t;

typedef struct{
	int status_code;
	enum response_code_type type;
	char *explain;
} response_code;

char *fly_code_explain(fly_rescode_t type); void fly_500_error(int c_sockfd, fly_pool_t *pool, char *version); 

typedef int fly_flag_t;
#endif
