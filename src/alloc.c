#include "alloc.h"
#include "util.h"
#include "err.h"
#include "rbtree.h"

struct fly_size_bytes fly_sizes[] = {
	{XS, 1},
	{S, 1000},
	{M, 100000},
	{L, 1000000},
	{XL, 10000000},
	{-1, -1},
};

__fly_direct_log __fly_static void *__fly_malloc(size_t size);
__fly_static void __fly_free(void *ptr);

ssize_t fly_bytes_from_size(fly_pool_s size)
{
	for (struct fly_size_bytes *s=fly_sizes; s->kb>=0; s++){
		if (s->size == size){
			return FLY_KB * s->kb;
		}
	}
	return -1;
}

__fly_direct_log void *fly_malloc(size_t size)
{
	return __fly_malloc(size);
}

__fly_direct_log __fly_static void *__fly_malloc(size_t size)
{
	void *res;
	res =  malloc(size);
	/* if failure to allocate memory, process is end. */
	if (res == NULL){
		fly_nomem_verror(
			errno,
			"no memory."
		);
	}
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
__fly_static int __fly_rb_search_block(fly_rbdata_t *k1, fly_rbdata_t *k2, fly_rbdata_t *cmpdata __fly_unused)
{
	void *__a1, *__a2;

	__a1 = fly_rbdata_ptr(k1);
	__a2 = fly_rbdata_ptr(k2);
	if (__a1 > __a2)
		return FLY_RB_CMP_BIG;
	else if (__a1 < __a2)
		return FLY_RB_CMP_SMALL;
	else
		return FLY_RB_CMP_EQUAL;
}

static void *__fly_palloc(fly_pool_t *pool, size_t size)
{
	fly_rbdata_t data, key;
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
	fly_rbdata_set_ptr(&data, new_block);
	fly_rbdata_set_ptr(&key, new_block->entry);
	if (fly_unlikely_null(fly_rb_tree_insert(pool->rbtree, &data, &key, NULL, NULL))){
		__fly_free(new_block->entry);
		__fly_free(new_block);
		return NULL;
	}

	fly_bllist_add_tail(&pool->blocks, &new_block->blelem);
	pool->block_size++;
	return new_block->entry;
}

struct fly_pool_manager *fly_pool_manager_init(void)
{
	struct fly_pool_manager *__pm;

	__pm = fly_malloc(sizeof(struct fly_pool_manager));
	if (fly_unlikely_null(__pm))
		return NULL;

	fly_bllist_init(&__pm->pools);
	__pm->total_pool_count = 0;
	return __pm;
}

void fly_release_all_pool(struct fly_pool_manager *__pm)
{
	if (__pm->total_pool_count == 0)
		return;

	struct fly_bllist *__b, *__n;
	struct fly_pool *__p;
	for (__b=__pm->pools.next; __b!=&__pm->pools; __b=__n){
		__n = __b->next;
		__p = fly_bllist_data(__b, struct fly_pool, pbelem);
		if (!__p->self_delete)
			fly_delete_pool(__p);
	}
}

void fly_pool_manager_release(struct fly_pool_manager *__pm)
{
	fly_release_all_pool(__pm);
	fly_free(__pm);
}

#include <stdio.h>
static fly_pool_t *__fly_create_pool(struct fly_pool_manager *__pm, size_t size){
	fly_pool_t *pool;
	pool = __fly_malloc(sizeof(fly_pool_t));
	pool->max = fly_max_size(size);
	pool->manager = __pm;

	fly_bllist_init(&pool->blocks);
	fly_bllist_init(&pool->pbelem);
	pool->block_size = 0;
	pool->rbtree = fly_rb_tree_init(__fly_rb_search_block);
	pool->self_delete = false;

	fly_bllist_add_tail(&__pm->pools, &pool->pbelem);
	__pm->total_pool_count++;
	return pool;
}

fly_pool_t *fly_create_pool(struct fly_pool_manager *__pm, fly_page_t	page){
	return __fly_create_pool(__pm, fly_byte_convert(page));
}
fly_pool_t *fly_create_poolb(struct fly_pool_manager *__pm, size_t	size){
	return __fly_create_pool(__pm, size);
}

void fly_pbfree(fly_pool_t *pool, void *ptr)
{
	fly_rb_node_t *__dn;
	fly_pool_b *__db;
	fly_rbdata_t key;

	if (pool->block_size == 0 || pool->rbtree->node_count == 0)
		return;

	fly_rbdata_set_ptr(&key, ptr);
	__dn = (fly_rb_node_t *) fly_rb_node_from_key(pool->rbtree, &key, NULL);
	if (fly_unlikely_null(__dn))
		return;

	__db = (fly_pool_b *) fly_rbdata_ptr(&__dn->data);
	fly_bllist_remove(&__db->blelem);
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
	if (fly_unlikely_null(pool)){
		FLY_EMERGENCY_ERROR(
			"alloc without pool error."
		);
	}
	return __fly_palloc(pool, size);
}

void fly_delete_pool(fly_pool_t *pool)
{
	struct fly_bllist *__b, *__n;

	fly_pool_b *block;

	for (__b=pool->blocks.next; __b!=&pool->blocks; __b=__n){
		__n = __b->next;
		block = fly_bllist_data(__b, struct fly_pool_block, blelem);

		__fly_free(block->entry);
		__fly_free(block);
		pool->block_size--;
	}

	fly_rb_tree_release(pool->rbtree);
	fly_bllist_remove(&pool->pbelem);
	pool->manager->total_pool_count--;
	__fly_free(pool);
}
