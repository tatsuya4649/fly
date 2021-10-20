#ifndef _URI_H
#define _URI_H

#include <stddef.h>
#include "char.h"

#define FLY_INDEX_PATH				"FLY_INDEX_PATH"
struct fly_uri{
	char *ptr;
	size_t len;
};

typedef struct fly_uri fly_uri_t;

#define fly_uri_set(__req, __ptr, __len)			\
	do {											\
		(__req)->request_line->uri.ptr = (__ptr);		\
		(__req)->request_line->uri.len = (__len);		\
	} while(0)
bool fly_is_uri_index(fly_uri_t *uri);

#endif
