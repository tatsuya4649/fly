#ifndef _ENCODE_H
#define _ENCODE_H
#include <stddef.h>
#include <string.h>
#include <ctype.h>
#include <string.h>
#include "util.h"

/* compress/decompress libraries */
#ifdef HAVE_ZLIB_H
#include <zlib.h>
#else
#error ZLIB NO_ERROR
#endif
#if defined HAVE_LIBBROTLIDEC && defined HAVE_LIBBROTLIENC
#include <brotli/encode.h>
#include <brotli/decode.h>
#endif

#include "alloc.h"
#include "bllist.h"
#include "buffer.h"

#define FLY_ENCODE_TYPE(x, p)		\
	{ fly_ ## x, #x, p, fly_ ## x ## _encode, fly_ ## x ## _decode }
#define FLY_ENCODE_NULL			{ -1, NULL, -1, NULL, NULL }
#define FLY_ENCODE_END(e)		((e)->name == NULL)
#define FLY_ENCODE_ASTERISK		{ fly_asterisk_encode, "*", 0, NULL, NULL }
#define FLY_ACCEPT_ENCODING_HEADER				"accept-encoding"
enum __fly_encoding_type{
#ifdef HAVE_ZLIB_H
	fly_gzip,
	fly_deflate,
#endif
	fly_identity,
#if defined HAVE_LIBBROTLIDEC && defined HAVE_LIBBROTLIENC
	fly_br,
#endif
	fly_asterisk_encode,
	fly_noencode,
};
typedef enum __fly_encoding_type fly_encoding_e;
typedef char fly_encname_t;
#ifdef HAVE_ZLIB_H
typedef Bytef fly_encbuf_t;
#endif


typedef ssize_t (*fly_send_t)(int c_sockfd, const void *buf, size_t buflen, int flag);


#include "event.h"
struct fly_response;
struct fly_encoding_type;

#define fly_etype_from_de(__de)			((__de)->etype)
struct fly_de{
	fly_pool_t						*pool;
	struct fly_encoding_type		*etype;
	enum{
		FLY_DE_ENCODE,
		FLY_DE_DECODE,
		FLY_DE_FROM_PATH,
	} type;
	fly_buffer_t					*encbuf;
//	int								encbuflen;
	fly_buffer_t					*decbuf;
//	int								decbuflen;
	int								c_sockfd;
	int								fd;
	/* where to start encoding of fd */
	off_t							offset;
	/* how many encoding count */
	size_t							count;
	/* byte from start for sending */
	int								bfs;
	fly_event_t						*event;
	struct fly_response				*response;

	char							*already_ptr;
	size_t							already_len;

	size_t							contlen;
	fly_bit_t						end : 1;
	fly_bit_t						target_already_alloc: 1;
	fly_bit_t						overflow: 1;
};
typedef struct fly_de fly_de_t;
struct fly_de *fly_de_init(fly_pool_t *pool);

typedef int (*fly_encode_t)(fly_de_t *de);
typedef int (*fly_decode_t)(fly_de_t *de);
struct fly_encoding_type{
	fly_encoding_e	type;
	fly_encname_t	*name;
	/* if same quality value, used */
	int				priority;

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

	struct fly_bllist		blelem;
	fly_bit_t				use: 1;
};

struct fly_request;
struct fly_reponse;
typedef struct fly_request fly_request_t;

struct fly_encoding{
	fly_pool_t *pool;

	struct fly_bllist			accepts;
	struct fly_request			*request;
	/* acceptable quantity */
	size_t						accept_count;
};
typedef struct fly_encoding fly_encoding_t;
#define FLY_ACCEPT_ENCODING_SUCCESS			0
#define FLY_ACCEPT_ENCODING_SYNTAX_ERROR		-1
#define FLY_ACCEPT_ENCODING_ERROR				-2
#define FLY_ACCEPT_ENCODING_NOT_ACCEPTABLE	-3
int fly_accept_encoding(struct fly_request *req);
bool fly_encoding_matching(struct fly_encoding *enc, struct fly_encoding_type *type);

#define FLY_ENCODE_SUCCESS			1
#define FLY_ENCODE_ERROR			-1
#define FLY_ENCODE_SEEK_ERROR		-2
#define FLY_ENCODE_TYPE_ERROR		-3
#define FLY_ENCODE_READ_ERROR		-4
#define FLY_ENCODE_BUFFER_ERROR		-5

#define FLY_DECODE_SUCCESS			1
#define FLY_DECODE_ERROR			-1
#define FLY_DECODE_SEEK_ERROR		-2
#define FLY_DECODE_TYPE_ERROR		-3
#define FLY_DECODE_READ_ERROR		-4
#define FLY_DECODE_BUFFER_ERROR		-5

#if defined HAVE_LIBBROTLIDEC && defined HAVE_LIBBROTLIENC
/* br encode/decode */
int fly_br_decode(fly_de_t *de);
int fly_br_encode(fly_de_t *de);
#endif

#ifdef HAVE_ZLIB_H
/* gzip encode/decode */
int fly_gzip_decode(fly_de_t *de);
int fly_gzip_encode(fly_de_t *de);

/* deflate encode/decode */
int fly_deflate_decode(fly_de_t *de);
int fly_deflate_encode(fly_de_t *de);
#endif

/* identity encode/decode */
int fly_identity_decode(fly_de_t *de);
int fly_identity_encode(fly_de_t *de);

#define FLY_ENCNAME_MAXLEN			(20)
/* 6 = int and point and 3 decimal places */
#define FLY_ENCQVALUE_MAXLEN		(6)

fly_encname_t *fly_decided_encoding_name(fly_encoding_t *enc);
fly_encoding_type_t *fly_decided_encoding_type(fly_encoding_t *enc);

fly_buffer_c *fly_e_buf_add(fly_de_t *de);
fly_buffer_c *fly_d_buf_add(fly_de_t *de);
struct fly_response;
int fly_esend_body(fly_event_t *e, struct fly_response *response);
struct fly_response;

fly_encoding_type_t *fly_supported_content_encoding(char *value);
void fly_de_release(fly_de_t *de);


#define fly_over_encoding_threshold(__ctx, __size)		\
			((__ctx)->response_encode_threshold <= (__size))
#define fly_over_encoding_threshold_from_response(__res)	\
			({		\
				struct fly_context *__ctx = (__res)->request->ctx;	\
				fly_over_encoding_threshold(__ctx, res->response_len); \
			 })

#define FLY_ENCODE_THRESHOLD			"FLY_ENCODE_THRESHOLD"
size_t fly_encode_threshold(void);

#endif
