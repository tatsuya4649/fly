#ifndef _LANG_H
#define _LANG_H

#include "util.h"
#include "bllist.h"

#define FLY_LANG_MAXLEN			(10)
#define FLY_LANG_QVALUE_MAX		1.000
#define FLY_LANG_QVALUE_MIN		0.000
#define FLY_LANG_ASTERISK		("*")
struct __fly_lang{
	char					lname[FLY_LANG_MAXLEN];
	float					quality_value;
	struct fly_bllist		blelem;
	fly_bit_t				asterisk: 1;
};

__unused static inline const char *fly_lang_name(struct __fly_lang *__l)
{
	return __l->asterisk ? FLY_LANG_ASTERISK : __l->lname;
}

struct fly_request;
typedef struct fly_request fly_request_t;
struct fly_lang{
	struct fly_bllist		langs;
	int						lang_count;
	fly_request_t			*request;
};
typedef struct fly_lang fly_lang_t;

int fly_accept_language(fly_request_t *req);
#define FLY_ACCEPT_LANG					("Accept-Language")

#endif
