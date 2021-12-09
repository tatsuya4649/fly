#ifndef _ALLOC_H
#define _ALLOC_H

#include <unistd.h>
#include <stdlib.h>
#include <inttypes.h>
#include "util.h"
#include "bllist.h"

enum fly_pool_size{
	XS,		/* max 1KB */
	S,		/* max 1MB */
	M,		/* max 100MB */
	L,		/* max 1GB */
	XL		/* max 10GB */
};

struct fly_size_bytes{
	enum fly_pool_size size;
	ssize_t kb;
};
#define FLY_KB		1000

extern struct fly_size_bytes fly_sizes[];

struct fly_pool_block;
struct fly_pool;
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
#define FLY_ALIGN_SIZE				(2*FLY_SIZEBIG(unsigned long, void *)) #define fly_align_ptr(ptr, asize)		(void *) (((uintptr_t) ptr + ((uintptr_t) asize-1)) & ~((uintptr_t) asize-1))

struct fly_rb_node;
struct fly_rb_tree;
struct fly_pool_block{
	void				*entry;
	void				*last;
	unsigned			size;				/* byte */
	struct fly_bllist	blelem;
};

struct fly_pool_manager{
	size_t					total_pool_count;
	struct fly_bllist		pools;
};
struct fly_pool_manager *fly_pool_manager_init(void);
void fly_pool_manager_release(struct fly_pool_manager *__pm);

struct fly_pool{
	fly_page_t				max;			/* per page size */
	fly_pool_t 				*current;	/* now pointed pool */

	struct fly_pool_manager *manager;
	struct fly_bllist		pbelem;

	struct fly_bllist		blocks;
	struct fly_rb_tree  	*rbtree;
	size_t					block_size;

	fly_bit_t				self_delete: 1;
};

#ifdef DEBUG
__fly_unused static struct fly_pool *fly_pool_debug(struct fly_bllist *__b)
{
	return (struct fly_pool *) fly_bllist_data(__b, struct fly_pool, pbelem);
}
#endif

void fly_release_all_pool(struct fly_pool_manager *__pm);
fly_pool_t *fly_create_pool(struct fly_pool_manager *__pm, fly_page_t size);
fly_pool_t *fly_create_poolb(struct fly_pool_manager *__pm, size_t size);
void fly_delete_pool(fly_pool_t *pool);
void *fly_palloc(fly_pool_t *pool, fly_page_t size);
void *fly_pballoc(fly_pool_t *pool, size_t size);
void fly_pfree(fly_pool_t *pool, void *ptr);
ssize_t fly_bytes_from_size(fly_pool_s size);
void fly_pbfree(fly_pool_t *pool, void *ptr);
void *fly_malloc(size_t size);
void fly_free(void *ptr);

#endif
