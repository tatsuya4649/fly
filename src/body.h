#ifndef _BODY_H
#define _BODY_H
#include "util.h"
#include "alloc.h"
#include "buffer.h"

typedef char fly_bodyc_t;
#define FLY_REQBODY_SIZE			(FLY_PAGESIZE*100)
struct fly_body{
	fly_pool_t			*pool;
	/* non null terminated */
	fly_bodyc_t			*body;
	int					body_len;
};

typedef struct fly_body fly_body_t;

struct fly_context;
fly_body_t *fly_body_init(struct fly_context *ctx);
void fly_body_release(fly_body_t *body);
void fly_body_setting(fly_body_t *body, char *ptr, size_t content_length);

fly_buffer_c *fly_get_body_buf(fly_buffer_t *buffer);
#include "encode.h"
fly_bodyc_t *fly_decode_body(fly_buffer_c *body_c, fly_encoding_type_t *t, fly_body_t *body, size_t content_length);
fly_bodyc_t *fly_decode_nowbody(fly_request_t *request, fly_encoding_type_t *t);
#define FLY_BODY_ENCBUF_PER_LEN			(1024*4)
#define FLY_BODY_ENCBUF_INIT_LEN		(1)
#define FLY_BODY_ENCBUF_CHAIN_MAX(__size)		((size_t) (((size_t) __size/FLY_BODY_ENCBUF_PER_LEN) + 1))

#define FLY_BODY_DECBUF_PER_LEN			(1024*4)
#define FLY_BODY_DECBUF_INIT_LEN		(1)
#define FLY_BODY_DECBUF_CHAIN_MAX(__size)		((size_t) (((size_t) __size/FLY_BODY_DECBUF_PER_LEN) + 1))
#endif
