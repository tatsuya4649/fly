#include "alloc.h"
#include "util.h"
#include "err.h"
#include "rbtree.h"

static fly_pool_t init_pool = {
	.next = &init_pool,
};

struct fly_size_bytes fly_sizes[] = {
	{XS, 1},
	{S, 1000},
	{M, 100000},
	{L, 1000000},
	{XL, 10000000},
	{-1, 0},
};

__direct_log __fly_static void *__fly_malloc(size_t size);
__fly_static void __fly_free(void *ptr);

ssize_t fly_bytes_from_size(fly_pool_s size)
{
	for (struct fly_size_bytes *s=fly_sizes; s->size>=0; s++){
		if (s->size == size){
			return FLY_KB * s->kb;
		}
	}
	return -1;
}

__direct_log void *fly_malloc(size_t size)
{
	return __fly_malloc(size);
}

__direct_log __fly_static void *__fly_malloc(size_t size)
{
	void *res;
	res =  malloc(size);
	/* if failure to allocate memory, process is end. */
	if (res == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_NOMEM,
			"no memory"
		);
	return res;
}

__fly_static void __fly_free(void *ptr)
{
	free(ptr);
	ptr = NULL;
}

void fly_free(void *ptr)
{
	__fly_free(ptr);
}

/* for red black tree */
__fly_static int __fly_rb_search_block(void *k1, void *k2)
{
	if (k1 > k2)
		return FLY_RB_CMP_BIG;
	else if (k1 < k2)
		return FLY_RB_CMP_SMALL;
	else
		return FLY_RB_CMP_EQUAL;
}

static void *__fly_palloc(fly_pool_t *pool, size_t size)
{
	fly_pool_b *new_block;

	new_block = __fly_malloc(sizeof(fly_pool_b));
	if (fly_unlikely_null(new_block))
		return NULL;

	new_block->entry = __fly_malloc(size);
	if (fly_unlikely_null(new_block->entry)){
		__fly_free(new_block);
		return NULL;
	}
	new_block->last = new_block->entry+size-1;
	new_block->size = size;
	new_block->next = pool->dummy;
	new_block->prev = pool->dummy;
	if (fly_unlikely_null(fly_rb_tree_insert(pool->rbtree, new_block, new_block->entry))){
		__fly_free(new_block->entry);
		__fly_free(new_block);
		return NULL;
	}

	if (pool->entry == pool->dummy){
		pool->entry = new_block;
		pool->dummy->next = pool->entry;
	}
	pool->block_size++;
	pool->last_block->next = new_block;
	new_block->prev = pool->last_block;
	pool->dummy->prev = new_block;
	pool->last_block = new_block;
	return new_block->entry;
}

#include <stdio.h>
static fly_pool_t *__fly_create_pool(size_t size){
	fly_pool_t *pool;
	pool = __fly_malloc(sizeof(fly_pool_t));
	if (pool == NULL)
		return NULL;
	pool->max = fly_max_size(size);
	pool->current = pool;
	pool->next = &init_pool;
	pool->block_size = 0;
	FLY_POOL_DUMMY_INIT(pool);
	if (fly_unlikely_null(pool->dummy))
		return NULL;
	pool->dummy->next = pool->dummy;
	pool->entry = pool->dummy;
	pool->last_block = pool->dummy;
	pool->rbtree = fly_rb_tree_init(__fly_rb_search_block);
	if (init_pool.next == &init_pool){
		init_pool.next = pool;
	}else{
		fly_pool_t *p;
		for (p=init_pool.next; p->next!=&init_pool; p=p->next)
			;
		p->next = pool;
	}
	return pool;
}

fly_pool_t *fly_create_pool(fly_page_t	page){
	return __fly_create_pool(fly_byte_convert(page));
}
fly_pool_t *fly_create_poolb(size_t	size){
	return __fly_create_pool(size);
}

void fly_pbfree(fly_pool_t *pool, void *ptr)
{
	fly_rb_node_t *__dn;
	fly_pool_b *__db;
	if (pool->block_size == 0 || pool->rbtree->node_count == 0)
		return;

	__dn = (fly_rb_node_t *) fly_rb_node_from_key(pool->rbtree, ptr);
	if (fly_unlikely_null(__dn))
		return;

	__db = (fly_pool_b *) __dn->data;
	__db->prev->next = __db->next;
	__db->next->prev = __db->prev;

	fly_rb_delete(pool->rbtree, __dn);
	__fly_free(__db->entry);
	__fly_free(__db);
	pool->block_size--;
	return;
}

void *fly_palloc(fly_pool_t *pool, fly_page_t psize)
{
	return __fly_palloc(pool, (size_t) fly_byte_convert(psize));
}

void *fly_pballoc(fly_pool_t *pool, size_t size)
{
	return __fly_palloc(pool, size);
}

void fly_delete_pool(fly_pool_t **pool)
{
	fly_pool_t *prev = NULL;
	fly_pool_t *next = NULL;
	for (fly_pool_t *p=init_pool.next; p!=&init_pool; p=next){
		next = p->next;
		if (p == *pool){
			/* only init_pool */
			if (next == &init_pool && prev == NULL)
				init_pool.next = &init_pool;
			/* init_pool */
			else if (prev == NULL)
				init_pool.next = p->next;
			else if (next == &init_pool)
				prev->next = &init_pool;
			/* others */
			else
				prev->next = p->next;

			if (p->block_size){
				fly_pool_b *nblock;
				for (fly_pool_b *block=p->dummy->next; block!=p->dummy;block=nblock){
					nblock = block->next;
					__fly_free(block->entry);
					__fly_free(block);
					p->block_size--;
				}
			}

			fly_rb_tree_release(p->rbtree);
			__fly_free(p->dummy);
			__fly_free(p);
			return;
		}
		prev = p;
	}
}
