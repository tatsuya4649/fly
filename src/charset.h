#ifndef _CHARSET_H
#define _CHARSET_H

#include "util.h"
#include "bllist.h"

#define FLY_CHARSET_MAXLEN			(30)
#define FLY_CHARSET_QVALUE_MAX		1.000
#define FLY_CHARSET_QVALUE_MIN		0.000
#define FLY_CHARSET_ASTERISK		("*")
struct __fly_charset{
	char				cname[FLY_CHARSET_MAXLEN];
	float				quality_value;

	struct fly_bllist	blelem;
	fly_bit_t			asterisk: 1;
};

__fly_unused static inline const char *fly_charset_name(struct __fly_charset *__c)
{
	return __c->asterisk ? FLY_CHARSET_ASTERISK	: __c->cname;
}

struct fly_request;
typedef struct fly_request fly_request_t;
struct fly_charset{
	struct fly_bllist	charsets;
	int					charset_count;
	fly_request_t		*request;
};
typedef struct fly_charset fly_charset_t;

#define FLY_ACCEPT_CHARSET_SUCCESS				0
#define FLY_ACCEPT_CHARSET_SYNTAX_ERROR			-1
#define FLY_ACCEPT_CHARSET_ERROR				-2
#define FLY_ACCEPT_CHARSET_NOT_ACCEPTABLE		-3
int fly_accept_charset(fly_request_t *req);
#define FLY_ACCEPT_CHARSET					("accept-charset")

#endif

