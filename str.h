#ifndef _FLY_STRING_H
#define _FLY_STRING_H

#include "alloc.h"

struct fly_string{
	fly_pool_t			*pool;
	char				*ptr;
	size_t				len;
};
typedef struct fly_string fly_str_t;
#define fly_string_from_char(__s, c)	\
	do{									\
		(__s).str = (c);				\
		(__s).len = strlen((c));		\
	} while(0)

#define fly_str_len(__str)		((__str)->len)
#define fly_str_ptr(__str)			((__str)->ptr)

static inline void fly_str_init(fly_str_t *str)
{
	str->pool = NULL;
	str->ptr = NULL;
	str->len = 0;
}

#endif
