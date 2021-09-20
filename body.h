#ifndef _BODY_H
#define _BODY_H
#include "util.h"
#include "alloc.h"

typedef char fly_bodyc_t;
#define FLY_REQBODY_SIZE			(FLY_PAGESIZE*100)
struct fly_body{
	fly_pool_t *pool;
	/* non null terminated */
	fly_bodyc_t *body;
	int body_len;
};

typedef struct fly_body fly_body_t;

fly_body_t *fly_body_init(void);
int fly_body_release(fly_body_t *body);
int fly_body_setting(fly_body_t *body, fly_bodyc_t *buffer, size_t content_length);

fly_bodyc_t *fly_get_body_ptr(char *buffer);
#include "encode.h"
fly_bodyc_t *fly_decode_body(fly_bodyc_t *bptr, fly_encoding_type_t *t, fly_body_t *body, size_t content_length);

#endif
