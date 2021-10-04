#ifndef _CHARSET_H
#define _CHARSET_H

#include "util.h"

#define FLY_CHARSET_MAXLEN			(30)
#define FLY_CHARSET_QVALUE_MAX		1.000
#define FLY_CHARSET_QVALUE_MIN		0.000
struct __fly_charset{
	char cname[FLY_CHARSET_MAXLEN];
	float quality_value;
	struct __fly_charset *next;
	fly_bit_t asterisk: 1;
};

struct fly_request;
typedef struct fly_request fly_request_t;
struct fly_charset{
	struct __fly_charset *charsets;
	int charqty;

	fly_request_t *request;
};
typedef struct fly_charset fly_charset_t;

int fly_accept_charset(fly_request_t *req);
#define FLY_ACCEPT_CHARSET					("Accept-Charset")
#define FLY_ACCEPT_CHARSET_SMALL			("accept-charset")

#endif

