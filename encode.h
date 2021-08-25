#ifndef _ENCODE_H
#define _ENCODE_H
#include <stddef.h>
#include <string.h>

#define FLY_ENCODE_TYPE(x)		{ x, #x }
#define FLY_ENCODE_NULL			{ 0, NULL }
#define FLY_ENCODE_END(e)		((e)->name == NULL)
enum fly_encoding{
	gzip,
	compress,
	deflate,
	identity,
	br,
};
typedef enum fly_encoding fly_encoding_e;

typedef char fly_encname_t;
struct fly_encoding_type{
	fly_encoding_e type;
	fly_encname_t *name;
};
typedef struct fly_encoding_type fly_encoding_t;

extern fly_encoding_t encodes[];

fly_encoding_t *fly_encode_from_type(fly_encoding_e type);
fly_encoding_t *fly_encode_from_name(fly_encname_t *name);

#endif
