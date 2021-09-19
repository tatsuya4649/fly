#ifndef _ENCODE_H
#define _ENCODE_H
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <zlib.h>
#include "util.h"
#include "alloc.h"

#define FLY_ENCODE_TYPE(x, p)		\
	{ fly_ ## x, #x, p, fly_ ## x ## _encode, fly_ ## x ## _decode }
#define FLY_ENCODE_NULL			{ -1, NULL, -1, NULL, NULL }
#define FLY_ENCODE_END(e)		((e)->name == NULL)
#define FLY_ENCODE_ASTERISK		{ fly_asterisk, "*", 0, NULL, NULL }
#define FLY_ACCEPT_ENCODING_HEADER		"Accept-Encoding"
enum __fly_encoding_type{
	fly_gzip,
//	fly_compress,
	fly_deflate,
	fly_identity,
//	fly_br,
	fly_asterisk,
};
typedef enum __fly_encoding_type fly_encoding_e;
typedef char fly_encname_t;
typedef Bytef fly_encbuf_t;

typedef int (*fly_encode_t)(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);
typedef int (*fly_decode_t)(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);

struct fly_encoding_type{
	fly_encoding_e	type;
	fly_encname_t	*name;
	/* if same quality value, used */
	int			   priority;

	fly_encode_t	encode;
	fly_decode_t	decode;
};
typedef struct fly_encoding_type fly_encoding_type_t;

fly_encoding_type_t *fly_encoding_from_type(fly_encoding_e);
fly_encoding_type_t *fly_encoding_from_name(fly_encname_t *name);
fly_encname_t *fly_encname_from_type(fly_encoding_e type);

struct fly_encoding;
struct __fly_encoding{
	struct fly_encoding		*encoding;
	fly_encoding_type_t		*type;
	/* 0~100% */
	int						quality_value;
	struct __fly_encoding	*next;
	fly_bit_t				use: 1;
};

struct fly_request;
typedef struct fly_request fly_request_t;

struct fly_encoding{
	fly_pool_t *pool;
	struct __fly_encoding		*accepts;
	struct fly_request			*request;
	/* acceptable quantity */
	size_t						actqty;
};
typedef struct fly_encoding fly_encoding_t;
int fly_accept_encoding(fly_request_t *req);

#define FLY_ENCODE_SUCCESS			1
#define FLY_ENCODE_OVERFLOW			0
#define FLY_ENCODE_ERROR			-1

/* gzip encode/decode */
int fly_gzip_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);
int fly_gzip_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);

/* deflate encode/decode */
int fly_deflate_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);
int fly_deflate_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);

/* identity encode/decode */
int fly_identity_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);
int fly_identity_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);

#define FLY_ENCNAME_MAXLEN			(20)
/* 6 = int and point and 3 decimal places */
#define FLY_ENCQVALUE_MAXLEN		(6)

#endif
