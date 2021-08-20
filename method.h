#ifndef _METHOD_H
#define _METHOD_H

#include <stddef.h>

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

typedef struct{
	char *name;
	enum method_type type;
} http_method;

extern http_method methods[];


#endif
