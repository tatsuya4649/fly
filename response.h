#ifndef _RESPONSE_H
#define _RESPONSE_H
#include "header.h"
#include "body.h"
#include "version.h"
#include "util.h"
#include "event.h"

#define RESPONSE_LENGTH_PER		1024
#define FLY_RESPONSE_POOL_PAGE		100
#define DEFAULT_RESPONSE_VERSION			"1.1"

typedef unsigned long long fly_flag_t;

enum status_code_type{
	/* 1xx Info */
	_100,
	_101,
	/* 2xx Succes */
	_200,
	_201,
	_202,
	_203,
	_204,
	_205,
	/* 3xx Redirect */
	_300,
	_301,
	_302,
	_303,
	_304,
	_307,
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
	_413,
	_414,
	_415,
	_416,
	_417,
	_426,
	/* 5xx Server Error */
	_500,
	_501,
	_502,
	_503,
	_504,
	_505
};
typedef enum status_code_type fly_stcode_t;

struct fly_http_response{
	fly_pool_t *pool;
	fly_stcode_t status_code;
	fly_version_e version;
	fly_hdr_ci *header;
	fly_body_t *body;
};

typedef struct fly_http_response fly_response_t;

fly_response_t *fly_response_init(void);
int fly_response_release(fly_response_t *response);

typedef struct{
	int status_code;
	enum status_code_type type;
	char *explain;
	char **required_header;
} fly_status_code;

char *fly_stcode_explain(fly_stcode_t type);

int fly_response( int c_sockfd, fly_response_t *response, fly_flag_t flag);

void fly_500_error(int c_sockfd, fly_version_e version);
void fly_414_error(int c_sockfd, fly_version_e version);
void fly_404_error(int c_sockfd, fly_version_e version);
void fly_400_error(int c_sockfd, fly_version_e version);
#define FLY_DEFAULT_HTTP_VERSION		V1_1
void fly_notfound_request_line(int c_sockfd, fly_version_e version);
void fly_notfound_request_method(int c_sockfd, fly_version_e version);
void fly_unmatch_request_method(int c_sockfd, fly_version_e version);
void fly_notfound_uri(int c_sockfd, fly_version_e version);
void fly_notfound_http_version(int c_sockfd, fly_version_e version);
void fly_unmatch_http_version(int c_sockfd, fly_version_e version);
void fly_nonumber_http_version(int c_sockfd, fly_version_e version);

int fly_4xx_error_event(fly_event_manager_t *, fly_sock_t, fly_stcode_t);
int fly_5xx_error_event(fly_event_manager_t *, fly_sock_t, fly_stcode_t);
#endif
