#ifndef _LANG_H
#define _LANG_H

#include "util.h"

#define FLY_LANG_MAXLEN			(10)
#define FLY_LANG_QVALUE_MAX		1.000
#define FLY_LANG_QVALUE_MIN		0.000
struct __fly_lang{
	char lname[FLY_LANG_MAXLEN];
	float quality_value;
	struct __fly_lang *next;
	fly_bit_t asterisk: 1;
};

struct fly_request;
typedef struct fly_request fly_request_t;
struct fly_lang{
	struct __fly_lang *langs;
	int langqty;

	fly_request_t *request;
};
typedef struct fly_lang fly_lang_t;

int fly_accept_language(fly_request_t *req);
#define FLY_ACCEPT_LANG					("Accept-Language")

#endif
