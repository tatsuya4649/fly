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
	_505,
	INVALID_STATUS_CODE,
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
		FLY_RESPONSE_TYPE_NOCONTENT
	} type;
	size_t						response_len;
	size_t						original_response_len;
	void						*send_ptr;
	int							 byte_from_start;
	size_t						 send_len;
	struct fly_mount_parts_file *pf;
	off_t						 offset;
	size_t						 count;
	struct fly_encoding_type	*encoding_type;
	fly_de_t					*de;			/* use response pool */
	fly_rcbs_t					*rcbs;
	union{
		int						datai;
		void *					datav;
		long					datal;
	};

	fly_bit_t					encoded: 1;
	fly_bit_t					blocking: 1;
};
typedef struct fly_response fly_response_t;
#define fly_disconnect_from_response(res)		((res)->request->connect->peer_closed = true)

#define fly_response_http_version_from_request(__res, __req)		\
	do{															\
		(__res)->version = (__req)->request_line->version->type;	\
	} while(0)

fly_response_t *fly_response_init(struct fly_context *ctx);
void fly_response_release(fly_response_t *response);

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
struct fly_response_content;
int fly_304_event(fly_event_t *e);
int fly_400_event_norequest(fly_event_t *e, fly_connect_t *conn);
int fly_400_event(fly_event_t *e, fly_request_t *req);
int fly_404_event(fly_event_t *e, fly_request_t *req);
int fly_405_event(fly_event_t *e, fly_request_t *req);
int fly_413_event(fly_event_t *e, fly_request_t *req);
int fly_414_event(fly_event_t *e, fly_request_t *req);
int fly_415_event(fly_event_t *e, fly_request_t *req);

fly_response_t *fly_304_response(fly_request_t *req, struct fly_mount_parts_file *pf);
fly_response_t *fly_400_response(fly_request_t *req);
fly_response_t *fly_404_response(fly_request_t *req);
fly_response_t *fly_405_response(fly_request_t *req);
fly_response_t *fly_413_response(fly_request_t *req);
fly_response_t *fly_414_response(fly_request_t *req);
fly_response_t *fly_415_response(fly_request_t *req);
fly_response_t *fly_500_response(fly_request_t *req);

int __fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf, int (*handler)(fly_event_t *e));
int fly_response_from_pf(fly_event_t *e, fly_request_t *req, struct fly_mount_parts_file *pf);

int fly_response_content_event_handler(fly_event_t *e);
struct fly_response_content{
	fly_request_t *request;
	struct fly_mount_parts_file *pf;
};

#define FLY_RESPONSE_SUCCESS			1
#define FLY_RESPONSE_READ_BLOCKING		2
#define FLY_RESPONSE_WRITE_BLOCKING		3
#define FLY_RESPONSE_ERROR				-1

/* default response content(static content) */
struct fly_response_content_by_stcode{
	fly_stcode_t				status_code;
	char						*content_path;
	int							fd;
	fly_mime_type_t				*mime;
	fly_de_t					*de;

	struct stat					fs;
	fly_encoding_e				encode_type;
	struct fly_bllist			blelem;
	fly_bit_t					encoded: 1;
};
#if defined HAVE_LIBZ
#define FLY_RCBS_DEFAULT_ENCODE_TYPE		fly_gzip
#else
#define FLY_RCBS_DEFAULT_ENCODE_TYPE		fly_identify
#endif
struct fly_response_content_by_stcode *fly_rcbs_init(fly_context_t *ctx);
typedef struct fly_response_content_by_stcode fly_rcbs_t;
fly_encoding_type_t *fly_decided_encoding_type(fly_encoding_t *enc);
int fly_response_log(fly_response_t *res, fly_event_t *e);
const char *fly_status_code_str_from_type(fly_stcode_t type);
fly_stcode_t fly_status_code_from_long(long __l);
fly_response_t *fly_respf(fly_request_t *req, struct fly_mount_parts_file *pf);

static inline bool fly_encode_do(fly_response_t *res)
{
	return (res->encoding_type != NULL) ? true : false;
}

#define FLY_MAX_RESPONSE_CONTENT_LENGTH	"FLY_MAX_RESPONSE_CONTENT_LENGTH"
int fly_response_content_max_length(void);
void fly_response_timeout_end_setting(fly_event_t *e, fly_response_t *res);

#define FLY_RESPONSE_DECBUF_INIT_LEN		(1)
#define FLY_RESPONSE_DECBUF_CHAIN_MAX		(1)
#define FLY_RESPONSE_DECBUF_PER_LEN		(1024*4)

#define FLY_RESPONSE_ENCBUF_INIT_LEN		(1)
#define FLY_RESPONSE_ENCBUF_PER_LEN		(1024*4)
#define FLY_RESPONSE_ENCBUF_CHAIN_MAX(__size)		((size_t) (((size_t) __size/FLY_RESPONSE_ENCBUF_PER_LEN) + 1))

#endif
