#ifndef _ENCODE_H
#define _ENCODE_H
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include <zlib.h>
#include "util.h"

#define FLY_ENCODE_TYPE(x)		{ fly_ ## x, #x }
#define FLY_ENCODE_NULL			{ 0, NULL }
#define FLY_ENCODE_END(e)		((e)->name == NULL)
#define FLY_ENCODE_ASTERISK		{ fly_asterisk, "*" }
enum fly_encoding{
	fly_gzip,
	fly_compress,
	fly_deflate,
	fly_identity,
	fly_br,
	fly_asterisk,
};
typedef enum fly_encoding fly_encoding_e;
typedef char fly_encname_t;
typedef Bytef fly_encbuf_t;

struct fly_encoding_type{
	fly_encoding_e type;
	fly_encname_t *name;
};
typedef struct fly_encoding_type fly_encoding_type_t;

fly_encoding_type_t *fly_encode_from_type(fly_encoding_e type);
fly_encoding_type_t *fly_encode_from_name(fly_encname_t *name);
fly_encname_t *fly_encname_from_type(fly_encoding_e type);


/* TODO: encoding */
struct fly_encoding{
};
typedef struct fly_encoding fly_encoding_t;
#include "request.h"
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
int fly_identify_decode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);
int fly_identify_encode(fly_encbuf_t *encbuf, size_t encbuflen, fly_encbuf_t *decbuf, size_t decbuflen);

#endif
