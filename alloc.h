#ifndef _ALLOC_H
#define _ALLOC_H

#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "util.h"

enum fly_pool_size{
	XS,		/* max 1KB */
	S,		/* max 1MB */
	M,		/* max 100MB */
	L,		/* max 1GB */
	XL		/* max 10GB */
};

struct fly_size_bytes{
	enum fly_pool_size size;
	unsigned kb;
};
#define FLY_KB		1000

extern struct fly_size_bytes fly_sizes[];

typedef enum fly_pool_size fly_pool_s;
typedef unsigned long fly_page_t;
typedef struct fly_pool fly_pool_t;
typedef struct fly_pool_block fly_pool_b;
typedef enum fly_pool_size fly_pool_e;

#define FLY_DEFAULT_ALLOC_PAGESIZE			100
#define fly_byte_convert(page)		(sysconf(_SC_PAGESIZE)*(page))
#define fly_page_convert(byte)		(byte/sysconf(_SC_PAGESIZE) + (byte%sysconf(_SC_PAGESIZE) ? 1 : 0))
#define fly_max_size(size)			(10*(size))

#define FLY_SIZEBIG(a, b)			(sizeof(a) > sizeof(b) ? sizeof(a) : sizeof(b))
#define FLY_ALIGN_SIZE				(2*FLY_SIZEBIG(unsigned long, void *))
#define fly_align_ptr(ptr, asize)		(void *) (((uintptr_t) ptr + ((uintptr_t) asize-1)) & ~((uintptr_t) asize-1))
extern fly_pool_t *init_pool;

struct fly_pool_block{
	void *entry;
	void *last;
	unsigned size;				/* byte */
	struct fly_pool_block *next;
};

struct fly_pool{
	fly_page_t max;			/* per page size */
	fly_pool_t *current;	/* now pointed pool */
	fly_pool_t *next;		/* next pool */
	fly_pool_b *entry;		/* actual memory */
	fly_pool_b *last_block;
	unsigned block_size;
	unsigned per_size;
};


fly_pool_t *fly_create_pool(fly_page_t size);
fly_pool_t *fly_create_poolb(size_t size);
int fly_delete_pool(fly_pool_t **pool);
void *fly_palloc(fly_pool_t *pool, fly_page_t size);
void *fly_pballoc(fly_pool_t *pool, size_t size);
void fly_pfree(fly_pool_t *pool, void *ptr);
ssize_t fly_bytes_from_size(fly_pool_s size);
void fly_pbfree(fly_pool_t *pool, void *ptr);

#endif
