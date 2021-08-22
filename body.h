#ifndef _BODY_H
#define _BODY_H
#include "util.h"
#include "alloc.h"

typedef char fly_bodyc_t;
#define FLY_REQBODY_SIZE			(FLY_PAGESIZE*100)
struct fly_body{
	fly_pool_t *pool;
	fly_bodyc_t *body;
	int body_len;
};

typedef struct fly_body fly_body_t;

fly_body_t *fly_body_init(void);
int fly_body_release(fly_pool_t *pool);
int fly_body_setting(fly_body_t *body, fly_bodyc_t *buffer);

fly_bodyc_t *fly_get_body_ptr(char *buffer);

#endif
