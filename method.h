#ifndef _METHOD_H
#define _METHOD_H

#include <stddef.h>
#include <ctype.h>
#include <string.h>

enum method_type{
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	CONNECT,
	OPTIONS,
	TRACE,
	PATCH,
};
typedef enum method_type fly_method_e;

struct fly_http_method{
	char *name;
	enum method_type type;
};
typedef struct fly_http_method fly_http_method_t;

extern fly_http_method_t methods[];

fly_http_method_t *fly_match_method_name(char *method_name);
fly_http_method_t *fly_match_method_type(fly_method_e method);

#endif
