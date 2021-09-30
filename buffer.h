#ifndef _BUFFER_H
#define _BUFFER_H
#include <stdbool.h>
#include <stddef.h>
#include "util.h"
#include "alloc.h"

#define FLY_BUF_FULL				0x01
#define FLY_BUF_HALF				0x02
#define FLY_BUF_EMPTY				0x03
typedef void *fly_buf_p;
struct fly_buffer;
struct fly_buffer_chain{
	struct fly_buffer *buffer;
	fly_buf_p ptr;
	fly_buf_p lptr;
	fly_buf_p use_ptr;
	fly_buf_p unuse_ptr;
	size_t len;
	size_t use_len;
	size_t unuse_len;

	struct fly_buffer_chain *next;
	struct fly_buffer_chain *prev;
	fly_bit_t status: 4;
};

struct fly_buffer{
	struct fly_buffer_chain *chain;
	struct fly_buffer_chain *lchain;
	size_t chain_count;
	fly_pool_t *pool;

	/* -1 => infinity */
	size_t chain_max;
	/* alloc chain size one by one */
	size_t per_len;
	size_t use_len;
};
#define first_chain				chain->next
#define first_ptr				chain->next->ptr
#define first_useptr			chain->next->use_ptr
#define lunuse_ptr				lchain->unuse_ptr
#define lunuse_len				lchain->unuse_len
typedef struct fly_buffer fly_buffer_t;
typedef struct fly_buffer_chain fly_buffer_c;
#define FLY_BUF_CHAIN_INFINITY		(-1)

#define FLY_BUF_ADD_CHAIN_SUCCESS	(1)
#define FLY_BUF_ADD_CHAIN_LIMIT		(0)
#define FLY_BUF_ADD_CHAIN_ERROR		(-1)

#define FLY_BUFFER_EMPTY(b)			\
	((b)->lchain->unuse_ptr == (b)->chain->ptr)

fly_buffer_t *fly_buffer_init(fly_pool_t *pool, size_t init_len, size_t chain_max, size_t per_len);
int fly_buffer_add_chain(fly_buffer_t *buffer);
int fly_update_buffer(fly_buffer_t *buf, size_t len);
#define FLY_BUFFER_MEMCMP_OVERFLOW				(-2)
#define FLY_BUFFER_MEMCMP_BIG					(1)
#define FLY_BUFFER_MEMCMP_SMALL					(-1)
#define FLY_BUFFER_MEMCMP_EQUAL					(0)
int fly_buffer_memcmp(char *dist, char *src, fly_buffer_c *__c, size_t maxlen);
char *fly_buffer_strstr(fly_buffer_c *__c, const char *str);
char *fly_buffer_strstr_after(fly_buffer_c *__c, const char *str);
ssize_t fly_buffer_ptr_len(fly_buffer_t *__b, fly_buf_p p1, fly_buf_p p2);
void fly_buffer_memcpy(char *dist, char *src, fly_buffer_c *__c, size_t len);
fly_buffer_c *fly_buffer_chain_from_ptr(fly_buffer_t *buffer, fly_buf_p ptr);

fly_buf_p fly_update_chain(fly_buffer_c **c, fly_buf_p p, size_t len);
fly_buf_p fly_update_chain_one(fly_buffer_c **c, fly_buf_p p);
void fly_buffer_chain_release(fly_buffer_c *__c);
void fly_buffer_chain_release_from_length(fly_buffer_c *__c, size_t len);

#endif
