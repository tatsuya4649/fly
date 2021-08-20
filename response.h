#ifndef _RESPONSE_H
#define _RESPONSE_H
#include "header.h"
#include "version.h"
#include "util.h"

#define RESPONSE_LENGTH_PER		1024
#define FLY_RESPONSE_POOL_PAGE		100
#define DEFAULT_RESPONSE_VERSION			"1.1"

typedef struct{
	char *status_line;
	char *header;
	int header_len;
	char *body;
	ssize_t body_len;
} http_response;

typedef int fly_flag_t;
int fly_response(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	fly_version_e version,
	char *header_lines,
	int header_len,
	char *body,
	ssize_t body_len,
	fly_flag_t flag
);
int fly_response_file(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	fly_version_e version,
	char *header_lines,
	int header_len,
	char *file_path,
	int mount_number,
	fly_pool_s size,
	fly_flag_t flag
);
fly_pool_t *fly_response_init(void);
int fly_response_release(fly_pool_t *respool);

enum response_code_type{
	/* 1xx Info */
	/* 2xx Succes */
	_200,
	/* 3xx Redirect */
	/* 4xx Client Error */
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
	/* 5xx Server Error */
	_500
};
typedef enum response_code_type fly_rescode_t;

typedef struct{
	int status_code;
	enum response_code_type type;
	char *explain;
} response_code;

char *fly_code_explain(fly_rescode_t type);
void fly_500_error(int c_sockfd, fly_pool_t *pool, fly_version_e version); 

#endif
