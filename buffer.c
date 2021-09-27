#include "buffer.h"

fly_buffer_t *fly_buffer_init(fly_pool_t *pool, size_t init_len, size_t chain_max, size_t per_len);
int fly_buffer_add_chain(fly_buffer_t *buffer);
__fly_static char *__fly_buffer_strstr(fly_buffer_c *__c, const char *str, int flag);

fly_buffer_t *fly_buffer_init(fly_pool_t *pool, size_t init_len, size_t chain_max, size_t per_len)
{
	fly_buffer_t *buffer;

	buffer = fly_pballoc(pool, sizeof(fly_buffer_t));
	if (fly_unlikely_null(buffer))
		return NULL;
	buffer->chain = NULL;
	buffer->lchain = NULL;
	buffer->chain_count = 0;
	buffer->pool = pool;
	buffer->chain_max = chain_max;
	buffer->per_len = per_len;
	buffer->use_len = 0;

	if (init_len){
		while (init_len--)
			switch (fly_buffer_add_chain(buffer)){
			case FLY_BUF_ADD_CHAIN_SUCCESS:
				break;
			case FLY_BUF_ADD_CHAIN_LIMIT:
				return NULL;
			case FLY_BUF_ADD_CHAIN_ERROR:
				return NULL;
			}
	}
	return buffer;
}

int fly_buffer_add_chain(fly_buffer_t *buffer)
{
	struct fly_buffer_chain *chain;
	fly_pool_t *__pool = buffer->pool;

	if (buffer->chain_count >= buffer->chain_max)
		return FLY_BUF_ADD_CHAIN_LIMIT;

	chain = fly_pballoc(__pool, sizeof(struct fly_buffer_chain));
	if (fly_unlikely_null(chain))
		return FLY_BUF_ADD_CHAIN_ERROR;
	chain->status = FLY_BUF_EMPTY; chain->len = buffer->per_len;
	chain->use_len = 0;
	chain->buffer = buffer;
	chain->unuse_len = chain->len;
	chain->ptr = fly_pballoc(__pool, chain->len);
	chain->lptr = chain->ptr + chain->len - 1;
	chain->unuse_ptr = chain->ptr;
	chain->next = NULL;
	if (fly_unlikely_null(chain))
		return FLY_BUF_ADD_CHAIN_ERROR;

	if (buffer->chain_count == 0)
		buffer->chain = chain;
	else
		buffer->lchain->next = chain;
	buffer->lchain = chain;
	buffer->chain_count++;
	return FLY_BUF_ADD_CHAIN_SUCCESS;
}

fly_buf_p fly_update_chain_one(fly_buffer_c **c, fly_buf_p p)
{
	return fly_update_chain(c, p, 1);
}

fly_buf_p fly_update_chain(fly_buffer_c **c, fly_buf_p p, size_t len)
{
	if ((*c)->lptr >= p+len)
		return p+len;
	else{
		ssize_t __len = (p+len)-(*c)->lptr;
		while((__len-=(*c)->len)){
			*c=(*c)->next;
			if (!*c)
				return NULL;
			if (__len < 0)
				break;
		}

		return (*c)->ptr+((*c)->len+__len-1);
	}
}

int fly_update_buffer(fly_buffer_t *buf, size_t len)
{
	struct fly_buffer_chain *__l;
	ssize_t i;

	__l = buf->lchain;
	i = len;
	while ( (i - (ssize_t) __l->unuse_len) >= 0 ){
		i -= (ssize_t) __l->unuse_len;
		__l->use_len = __l->len;
		__l->unuse_len = 0;
		__l->status = FLY_BUF_FULL;
		switch (fly_buffer_add_chain(buf)){
		case FLY_BUF_ADD_CHAIN_SUCCESS:
			break;
		case FLY_BUF_ADD_CHAIN_LIMIT:
			return FLY_BUF_ADD_CHAIN_LIMIT;
		case FLY_BUF_ADD_CHAIN_ERROR:
			return FLY_BUF_ADD_CHAIN_ERROR;
		}

		__l = buf->lchain;
	}

	/* if i == 0, not use */
	if (i){
		__l->use_len += (size_t) i;
		__l->unuse_len -= (size_t) i;
		__l->status = FLY_BUF_HALF;
		__l->unuse_ptr += (size_t) i;
	}

	return 0;
}

__fly_static fly_buf_p *__fly_bufp_inc(fly_buffer_c **__c, fly_buf_p *ptr)
{
	fly_buf_p res = *ptr;
	if ((*__c)->lptr < *ptr+1){
		*ptr = (*__c)->next->ptr;
		*__c = (*__c)->next;
	}else
		*ptr = *ptr+1;

	return res;
}

static inline bool __fly_bufp_end(fly_buffer_c *__c, fly_buf_p ptr)
{
	if (__c->next == NULL && ptr >= __c->lptr)
		return true;
	else
		return false;
}

#define FLY_BUFFER_STRSTR_AFTER			(1<<0)
__fly_static char *__fly_buffer_strstr(fly_buffer_c *__c, const char *str, int flag)
{
	fly_buf_p n;

	n = __c->ptr;
	for (;;){
		const char *s = str;
		while (*(char *) __fly_bufp_inc(&__c , &n) == *s++){
			if (!*s){
				if (flag & FLY_BUFFER_STRSTR_AFTER){
					return n;
				}else
					return n-strlen(str);
			}
		}

		if (__fly_bufp_end(__c, n))
			break;
	}

	return NULL;
}

char *fly_buffer_strstr(fly_buffer_c *__c, const char *str)
{
	return __fly_buffer_strstr(__c, str, 0);
}

char *fly_buffer_strstr_after(fly_buffer_c *__c, const char *str)
{
	return __fly_buffer_strstr(__c, str, FLY_BUFFER_STRSTR_AFTER);
}

/* like return length of p1 - p2 */
ssize_t fly_buffer_ptr_len(fly_buffer_t *__b, fly_buf_p p1, fly_buf_p p2)
{
	ssize_t len=0;
	bool p1_f=false, p2_f=false;
	fly_buffer_c *c=__b->chain;
	fly_buf_p ptr;

	ptr = c->ptr;
	while(true){
		/* p1 found, p2 not found*/
		if (ptr == p1){
			p1_f = true;
		}else if (ptr == p2){
			p2_f = true;
		}

		if (p1_f && p2_f){
			return len;
		}

		if (p1_f && !p2_f)
			len--;
		else if (p2_f && !p1_f)
			len++;

		if (ptr >= c->lptr){
			if (c->next){
				c = c->next;
				ptr = c->ptr;
			}else
				return 0;
		}else
			ptr++;
	}
}


void fly_buffer_memcpy(char *dist, char *src, fly_buffer_c *__c, size_t len)
{
	char *sptr;
	sptr = src;

	while(sptr<(char *) __c->ptr || sptr>(char *) __c->lptr){
		__c=__c->next;
		if (!__c)
			return;
	}

	while(len--){
		*dist++ = *sptr++;
		if (sptr > (char *) __c->lptr){
			__c = __c->next;
			sptr = __c->ptr;
		}
	}

	return;
}

fly_buffer_c *fly_buffer_chain_from_ptr(fly_buffer_t *buffer, fly_buf_p ptr)
{
	if (buffer->chain_count == 0)
		return NULL;
	for (fly_buffer_c *__c=buffer->chain; __c; __c=__c->next){
		if (__c->ptr<=ptr && __c->lptr>=ptr)
			return __c;
	}
	return NULL;
}
