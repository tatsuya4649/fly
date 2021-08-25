#include "alloc.h"
#include "util.h"

fly_pool_t *init_pool = NULL;

struct fly_size_bytes fly_sizes[] = {
	{XS, 1},
	{S, 1000},
	{M, 100000},
	{L, 1000000},
	{XL, 10000000},
	{-1, 0},
};

ssize_t fly_bytes_from_size(fly_pool_s size)
{
	for (struct fly_size_bytes *s=fly_sizes; s->size>=0; s++){
		if (s->size == size){
			return FLY_KB * s->kb;
		}
	}
	return -1;
}

void *fly_malloc(int size)
{
	return malloc(size);
}

int fly_memalign_page(void **ptr, fly_page_t page_size)
{
	return posix_memalign(ptr, fly_align_size(), fly_byte_convert(page_size));
}

int fly_memalign(void **ptr, int size)
{
	return posix_memalign(ptr, fly_align_size(), size);
}

void fly_free(void *ptr)
{
	free(ptr);
}

static void *__fly_palloc(fly_pool_t *pool, size_t size)
{
	fly_pool_b *new_block;
	new_block = fly_malloc(sizeof(fly_pool_b));
	if (new_block == NULL)
		return NULL;
	
	if (fly_memalign(&new_block->entry, size) != 0){
		fly_free(new_block);
		return NULL;
	}
	new_block->last = new_block->entry+size-1;
	new_block->size = size;
	new_block->next = NULL;

	if (pool->entry == NULL)
		pool->entry = new_block;
	else
		pool->last_block->next = new_block;

	pool->block_size++;
	pool->last_block = new_block;
	return new_block->entry;
}

static fly_pool_t *__fly_create_pool(size_t size){
	fly_pool_t *pool;
	pool = fly_malloc(sizeof(fly_pool_t));
	if (pool == NULL)
		return NULL;
	pool->max = fly_max_size(size);
	pool->current = pool;
	pool->next = NULL;
	pool->entry = NULL;
	pool->block_size = 0;
	if (init_pool == NULL){
		init_pool = pool;
	}else{
		fly_pool_t *p;
		for (p=init_pool; p->next!=NULL; p=p->next)
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


void *fly_palloc(fly_pool_t *pool, fly_page_t psize)
{
	return __fly_palloc(pool, (size_t) fly_byte_convert(psize));
}

void *fly_pballoc(fly_pool_t *pool, size_t size)
{
	return __fly_palloc(pool, size);
}

int fly_delete_pool(fly_pool_t *pool)
{
	fly_pool_t *prev = NULL;
	fly_pool_t *next = NULL;
	for (fly_pool_t *p=init_pool; p!=NULL; p=next){
		next = p->next;
		if (p == pool){
			if (prev!=NULL)
				prev->next = p->next;
			fly_pool_b *nblock;
			for (fly_pool_b *block=p->entry; block!=NULL;block=nblock){
				nblock = block->next;
				fly_free(block->entry);
				fly_free(block);
				p->block_size--;
			}
			fly_free(p);
			return 0;
		}
		prev = p;
	}
	return -1;
}

int fly_pfree(__unused fly_pool_t *pool, __unused void *ptr)
{
	return 0;
}
