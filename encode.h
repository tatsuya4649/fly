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


typedef ssize_t (*fly_send_t)(int c_sockfd, const void *buf, size_t buflen, int flag);
//typedef int (*fly_send_blocking_t)(fly_event_t *e, fly_response_t *res);


#include "event.h"
struct fly_response;
struct fly_encoding_type;

#define FLY_SEND_FROM_PF_BUFLEN				(1024)
struct fly_de_buf{
	fly_encbuf_t buf[FLY_SEND_FROM_PF_BUFLEN];
	size_t buflen;
	size_t uselen;
	struct fly_de_buf *next;
	fly_encbuf_t *ptr;
	fly_bit_t status: 4;
};
#define FLY_DE_BUF_FULL				0x01
#define FLY_DE_BUF_HALF				0x02
#define FLY_DE_BUF_EMPTY			0x03
#define fly_de_buf_full(b)			((b)->status = FLY_DE_BUF_FULL)
#define fly_de_buf_half(b)			((b)->status = FLY_DE_BUF_HALF)
#define fly_de_buf_empty(b)			((b)->status = FLY_DE_BUF_EMPTY)

struct fly_de{
	fly_pool_t *pool;
	struct fly_encoding_type *etype;
	enum{
		FLY_DE_ENCODE,
		FLY_DE_DECODE,
		FLY_DE_ESEND,
		FLY_DE_ESEND_FROM_PATH,
	} type;
	enum{
		FLY_DE_INIT,
		FLY_DE_DE,
		FLY_DE_ENDING,
		FLY_DE_END,
	} fase;
	struct fly_de_buf *encbuf;
	int encbuflen;
	struct fly_de_buf *lencbuf;
	struct fly_de_buf *decbuf;
	int decbuflen;
	struct fly_de_buf *ldecbuf;
	int c_sockfd;
	int fd;
	/* where to start encoding of fd */
	off_t offset;
	/* how many encoding count */
	size_t count;
	/* byte from start for sending */
	int bfs;
	fly_send_t send;
	struct fly_de_buf *send_ptr;
	fly_event_t *event;
	struct fly_response *response;

	size_t				contlen;
	fly_bit_t			end : 1;
};
typedef struct fly_de fly_de_t;

typedef int (*fly_encode_t)(fly_de_t *de);
typedef int (*fly_decode_t)(fly_de_t *de);
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
int fly_gzip_decode(fly_de_t *de);
int fly_gzip_encode(fly_de_t *de);

/* deflate encode/decode */
int fly_deflate_decode(fly_de_t *de);
int fly_deflate_encode(fly_de_t *de);

/* identity encode/decode */
int fly_identity_decode(fly_de_t *de);
int fly_identity_encode(fly_de_t *de);

#define FLY_ENCNAME_MAXLEN			(20)
/* 6 = int and point and 3 decimal places */
#define FLY_ENCQVALUE_MAXLEN		(6)

fly_encname_t *fly_decided_encoding_name(fly_encoding_t *enc);
fly_encoding_type_t *fly_decided_encoding_type(fly_encoding_t *enc);

struct fly_de_buf *fly_d_buf_add(fly_de_t *de);
struct fly_de_buf *fly_e_buf_add(fly_de_t *de);
struct fly_response;
int fly_esend_body(fly_event_t *e, struct fly_response *response);
#endif
