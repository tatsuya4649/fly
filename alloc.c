#include "alloc.h"

fly_pool_t *init_pool = NULL;

void *fly_malloc(int size)
{
	return malloc(size);
}

int fly_memalign(void **ptr, int page_size)
{
	return posix_memalign(ptr, fly_align_size(), fly_byte_convert(page_size));
}

void fly_free(void *ptr)
{
	free(ptr);
}

fly_pool_t *fly_create_pool(
	fly_page_t	size
){
	fly_pool_t *pool;
	pool = fly_malloc(sizeof(fly_pool_t));
	if (pool == NULL)
		return NULL;
	pool->max = fly_max_size(size);
	pool->current = pool;
	pool->next = NULL;
	/* actual memory */
	pool->entry = fly_malloc(sizeof(fly_pool_b));
	pool->entry->size = fly_byte_convert(size);
	pool->entry->entry = fly_malloc(pool->entry->size);
	pool->entry->last = pool->entry->entry;
	pool->entry->next = NULL;
	if (pool->entry->entry == NULL)
		return NULL;

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

void *fly_palloc(fly_pool_t *pool, fly_page_t size)
{
	fly_pool_b *block;
	for (block=pool->entry; block!=NULL; block=block->next){
		void *start_ptr = fly_align_ptr(block->last+1, fly_align_size());
		if (block->size-(start_ptr-block->entry) > (long) fly_byte_convert(size)){
			block->last = start_ptr + fly_byte_convert(size);
			return start_ptr;
		}
	}
	fly_pool_b *new_block;
	new_block = fly_malloc(sizeof(fly_pool_b));
	if (new_block == NULL)
		return NULL;

	if (fly_memalign(new_block->entry, size) != 0){
		fly_free(new_block);
		return NULL;
	}
	new_block->last = new_block->entry;
	new_block->size = fly_byte_convert(size);
	new_block->next = NULL;
	block->next = new_block;
	
	return new_block->entry;
}

void *fly_pballoc(fly_pool_t *pool, size_t size)
{
	return fly_palloc(pool, fly_page_convert(size));
}

int fly_delete_pool(fly_pool_t *pool)
{
	fly_pool_t *prev = NULL;
	fly_pool_t *next = NULL;
	for (fly_pool_t *p=init_pool; p!=NULL; p=next){
		next = p->next;
		if (p == pool){
			if (prev!=NULL){
				prev->next = p->next;
			}
			fly_pool_b *next_block;
			for (fly_pool_b *block=p->entry; block!=NULL; block=next_block){
				next_block = block->next;
				fly_free(block->entry);
				fly_free(block);
			}
			fly_free(p);
			return 0;
		}
		prev = p;
	}
	return -1;
}
