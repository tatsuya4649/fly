#ifndef _BUFFER_H
#define _BUFFER_H
#include <stdbool.h>
#include <stddef.h>
#include "util.h"
#include "alloc.h"
#include "bllist.h"

#define FLY_BUF_FULL				0x01
#define FLY_BUF_HALF				0x02
#define FLY_BUF_EMPTY				0x03
typedef void *fly_buf_p;
struct fly_buffer;
struct fly_buffer_chain{
	struct fly_buffer	*buffer;
	fly_buf_p			ptr;
	fly_buf_p 			lptr;
	fly_buf_p 			use_ptr;
	fly_buf_p 			unuse_ptr;
	size_t				len;
	size_t 				use_len;
	size_t 				unuse_len;

	struct fly_bllist	blelem;
	fly_bit_t			status: 4;
};

struct fly_buffer{
	struct fly_bllist		chain;
	size_t chain_count;
	fly_pool_t *pool;

	/* -1 => infinity */
	size_t chain_max;
	/* alloc chain size one by one */
	size_t per_len;
	size_t use_len;
};

#ifdef DEBUG
	#define FLY_BUFFER_DEBUG_CHAIN_COUNT(__b)		\
			assert((__b)->chain_count > 0)

__unused static struct fly_buffer_chain *fly_buffer_chain_debug(struct fly_bllist *__b)
{
	return (struct fly_buffer_chain *) fly_bllist_data(__b, struct fly_buffer_chain, blelem);
}

#else
	#define FLY_BUFFER_DEBUG_CHAIN_COUNT(__b)
#endif
typedef struct fly_buffer fly_buffer_t;
typedef struct fly_buffer_chain fly_buffer_c;

static inline fly_buffer_c *fly_buffer_chain(struct fly_bllist *__b)
{
	return fly_bllist_data(__b, struct fly_buffer_chain, blelem);
}

static inline bool fly_is_chain_buffer_chain(struct fly_buffer *buf, struct fly_bllist *__b)
{
	return (&buf->chain == __b) ? true : false;
}

static inline bool fly_is_chain_term(struct fly_buffer_chain *__c)
{
	return fly_is_chain_buffer_chain(__c->buffer, __c->blelem.next);
}


static inline fly_buffer_c *fly_buffer_next_chain(fly_buffer_c *__c)
{
	if (fly_is_chain_term(__c))
		return NULL;
	return fly_buffer_chain(__c->blelem.next);
}

static inline fly_buffer_c *fly_buffer_first_chain(fly_buffer_t *__b)
{
	FLY_BUFFER_DEBUG_CHAIN_COUNT(__b);
	return fly_buffer_chain(__b->chain.next);
}

static inline fly_buffer_c *fly_buffer_last_chain(fly_buffer_t *__b)
{
	FLY_BUFFER_DEBUG_CHAIN_COUNT(__b);
	return fly_buffer_chain(__b->chain.prev);
}

static inline fly_buf_p fly_buffer_first_ptr(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_first_chain(__b);
	return __c->ptr;
}

static inline fly_buf_p fly_buffer_first_useptr(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_first_chain(__b);
	return __c->use_ptr;
}

static inline fly_buf_p fly_buffer_lunuse_ptr(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_last_chain(__b);
	return __c->unuse_ptr;
}

static inline size_t fly_buffer_lunuse_len(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_last_chain(__b);
	return __c->unuse_len;
}

static inline size_t fly_buffer_luse_len(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_last_chain(__b);
	return __c->use_len;
}

static inline fly_buf_p fly_buffer_luse_ptr(fly_buffer_t *__b)
{
	fly_buffer_c *__c = fly_buffer_last_chain(__b);
	return __c->use_ptr;
}


#define fly_buf_act_len(c)		(((c)->lptr-(c)->use_ptr)+1)

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
void fly_buffer_memcpy_all(char *dist, fly_buffer_t *__t);
void fly_buffer_memncpy_all(char *dist, fly_buffer_t *__t, size_t n);
fly_buffer_c *fly_buffer_chain_from_ptr(fly_buffer_t *buffer, fly_buf_p ptr);

fly_buf_p fly_update_chain(fly_buffer_c **c, fly_buf_p p, size_t len);
fly_buf_p fly_update_chain_one(fly_buffer_c **c, fly_buf_p p);
void fly_buffer_chain_release(fly_buffer_c *__c);
void fly_buffer_chain_release_from_length(fly_buffer_c *__c, size_t len);
void fly_buffer_release(fly_buffer_t *buf);
fly_buffer_c *fly_get_buf_chain(fly_buffer_t *buf, int i);
void fly_buffer_chain_refresh(fly_buffer_c *__c);

#define FLY_LEN_UNTIL_CHAIN_LPTR(__c , __p)		\
		(((__c) != fly_buffer_last_chain((__c)->buffer)) ? \
	((void *) (__c)->unuse_ptr - (void *) (__p) + 1) : \
	((void *) (__c)->unuse_ptr - (void *) (__p)))


#endif
