#ifndef _CONTEXT_H
#define _CONTEXT_H

#include "alloc.h"
#include "server.h"

#define FLY_CONTEXT_POOL_SIZE			10

typedef int fly_sock_t;
struct fly_context{
	fly_pool_t *pool;
	fly_sockinfo_t *listen_sock;
};
typedef struct fly_context fly_context_t;

fly_context_t *fly_context_init(void);
int fly_context_release(fly_context_t *ctx);

#endif
