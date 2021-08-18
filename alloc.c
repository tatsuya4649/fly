#include "alloc.h"

fly_pool_t *init_pool = NULL;

void *fly_malloc(int size)
{
	return malloc(size);
}

int fly_memalign(void **ptr, int page_size)
{
	return posix_memalign(ptr, fly_align_size(), fly_page_convert(page_size));
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
	pool->entry.size = fly_page_convert(size);
	pool->entry.entry = fly_malloc(size);
	pool->entry.last = pool->entry.entry;
	pool->entry.next = NULL;
	if (pool->entry.entry == NULL)
		return NULL;

	fly_pool_t *p;
	for (p=init_pool; p!=NULL; p=p->next)
		;
	p = pool;
	return pool;
}

void *fly_palloc(fly_pool_t *pool, fly_page_t size)
{
	fly_pool_b *block = &pool->entry;
	for (; block!=NULL; block=block->next){
		void *last_next = block->last+1;
		if (block->size-(fly_align_ptr(last_next, fly_align_size())-(block->entry)) <= (long) fly_page_convert(size)){
			return fly_align_ptr(last_next, fly_align_size());
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
	new_block->size = fly_page_convert(size);
	new_block->next = NULL;
	block->next = new_block;
	
	return new_block->entry;
}

int fly_pdelete(fly_pool_t *pool)
{
	fly_pool_t *prev = NULL;
	for (fly_pool_t *p=init_pool; p!=NULL; p=p->next){
		if (p == pool){
			if (prev!=NULL){
				prev->next = p->next;
			}
			
			for (fly_pool_b *block=&p->entry; block!=NULL; block=block->next){
				fly_free(block->entry);
			}
			fly_free(p);
		}
		prev = p;
		return 0;
	}
	return -1;
}
