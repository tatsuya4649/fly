#ifndef _RESPONSE_H
#define _RESPONSE_H

#include "request.h"
#include "header.h"
#include "body.h"
#include "version.h"
#include "util.h"
#include "event.h"
#include "log.h"
#include "fly.h"

#define RESPONSE_LENGTH_PER		1024
#define FLY_RESPONSE_POOL_PAGE		100
#define DEFAULT_RESPONSE_VERSION			"1.1"
#define FLY_DEFAULT_CONTENT_PATH_LEN		(30)

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
	_300, _301, _302, _303,
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
#define FLY_PATH_FROM_STATIC(p)			(__FLY_PATH_FROM_ROOT(static) "/" # p)

#include "mount.h"
struct fly_response_content_by_stcode;
typedef struct fly_response_content_by_stcode fly_rcbs_t;
struct fly_response{
	/* use pool: pool, header, body */
	fly_pool_t					*pool;
	fly_stcode_t				 status_code;
	fly_version_e				 version;
	fly_hdr_ci					*header;	/* use header pool */
	fly_body_t					*body;		/* usr body pool */
	fly_request_t				*request;

	enum{
		FLY_RESPONSE_READY,
		FLY_RESPONSE_STATUS_LINE,
		FLY_RESPONSE_HEADER,
		FLY_RESPONSE_CRLF,
		FLY_RESPONSE_BODY,
		FLY_RESPONSE_RELEASE,
		/* for v2 */
		FLY_RESPONSE_FRAME_HEADER,
		FLY_RESPONSE_DATA_FRAME,
	} fase;

	enum{
		FLY_RESPONSE_TYPE_ENCODED,
		FLY_RESPONSE_TYPE_BODY,
		FLY_RESPONSE_TYPE_PATH_FILE,
		FLY_RESPONSE_TYPE_DEFAULT,
	} type;
	void						*send_ptr;
	int							 byte_from_start;
	size_t						 send_len;
	struct fly_mount_parts_file *pf;
	off_t						 offset;
	size_t						 count;
	fly_encoding_t				*encoding;
	fly_de_t					*de;			/* use response pool */
	fly_rcbs_t					*rcbs;
	union{
		int						datai;
		void *					datav;
		long					datal;
	};

	fly_bit_t					 encoded: 1;
	fly_bit_t					 blocking: 1;
};
typedef struct fly_response fly_response_t;

fly_response_t *fly_response_init(void);
int fly_response_release(fly_response_t *response);

typedef struct{
	int status_code;
	enum status_code_type type;
	const char *status_code_str;
	char *explain;
	char *default_path;
	char **required_header;
} fly_status_code;
#define FLY_STATUS_CODE(n)			n, _ ## n , #n

char *fly_stcode_explain(fly_stcode_t type);

int fly_response_event(fly_event_t *e);

#define FLY_DEFAULT_HTTP_VERSION		V1_1
#define FLY_RESPONSE_LOG_LENGTH			300
#define FLY_RESPONSE_NONSTRING			""

struct fly_itm_response{
	fly_stcode_t status_code;
	fly_request_t *req;
};
typedef struct fly_itm_response fly_itm_response_t;
//int fly_4xx_error_event(fly_event_t *, fly_request_t *, fly_stcode_t);
//int fly_5xx_error_event(fly_event_t *, fly_request_t *, fly_stcode_t);
struct fly_response_content;
int fly_304_event(fly_event_t *e);
int fly_400_event(fly_event_t *e, fly_request_t *req);
int fly_404_event(fly_event_t *e, fly_request_t *req);
int fly_405_event(fly_event_t *e, fly_request_t *req);
int fly_414_event(fly_event_t *e, fly_request_t *req);
int fly_415_event(fly_event_t *e, fly_request_t *req);
int __fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf, int (*handler)(fly_event_t *e));
int fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf);

int fly_response_content_event_handler(fly_event_t *e);
struct fly_response_content{
	fly_request_t *request;
	struct fly_mount_parts_file *pf;
};

#define FLY_RESPONSE_SUCCESS			1
#define FLY_RESPONSE_BLOCKING			0
#define FLY_RESPONSE_ERROR				-1

/* default response content(static content) */
struct fly_response_content_by_stcode{
	fly_stcode_t status_code;
	char *content_path;
	int fd;
	fly_mime_type_t *mime;

	struct fly_response_content_by_stcode *next;
};
typedef struct fly_response_content_by_stcode fly_rcbs_t;
int __fly_encode_do(fly_response_t *res);
fly_encoding_type_t *fly_decided_encoding_type(fly_encoding_t *enc);
int __fly_response_log(fly_response_t *res, fly_event_t *e);
const char *fly_status_code_str_from_type(fly_stcode_t type);
fly_response_t *fly_respf(fly_request_t *req, struct fly_mount_parts_file *pf);

#endif
