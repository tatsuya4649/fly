#ifndef _URI_H
#define _URI_H

#include <stddef.h>
#include <stdint.h>
#include "char.h"
#include "bllist.h"

#define FLY_INDEX_PATH				"FLY_INDEX_PATH"
struct fly_uri{
	char *ptr;
	size_t len;
};

enum fly_path_param_type{
	FLY_PPT_INT		=0,
	FLY_PPT_FLOAT	=1,
	FLY_PPT_BOOL	=2,
	FLY_PPT_STR		=3,
	FLY_PPT_UNKNOWN	=4,
};

struct fly_param{
	char						*var_ptr;
	size_t						var_len;
	int							param_number;
	uint8_t						*value;
	size_t						value_len;
	enum fly_path_param_type	type;
	struct fly_bllist			blelem;
};

struct fly_path_param{
	int							param_count;
	struct fly_bllist			params;
};
#define fly_path_param_count(__p)		\
			((__p)->param_count)

typedef struct fly_uri fly_uri_t;

#define fly_uri_set(__req, __ptr, __len)			\
	do {											\
		(__req)->request_line->uri.ptr = (__ptr);		\
		(__req)->request_line->uri.len = (__len);		\
	} while(0)
bool fly_is_uri_index(fly_uri_t *uri);
struct fly_request;
int fly_query_parse_from_uri(struct fly_request *req, fly_uri_t *uri);

#endif
