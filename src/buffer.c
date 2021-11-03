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

	fly_bllist_init(&buffer->chain);
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

void fly_buffer_release(fly_buffer_t *buf)
{
	fly_pool_t *pool;

	pool = buf->pool;
	if (buf->chain_count == 0)
		return;

	while(buf->chain_count)
		fly_buffer_chain_release(fly_buffer_chain(buf->chain.next));

	fly_pbfree(pool, buf);
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
	chain->status = FLY_BUF_EMPTY;
	chain->len = buffer->per_len;
	chain->use_len = 0;
	chain->buffer = buffer;
	chain->unuse_len = chain->len;
	chain->ptr = fly_pballoc(__pool, chain->len);
	if (fly_unlikely_null(chain->ptr))
		return FLY_BUF_ADD_CHAIN_ERROR;
	chain->use_ptr = chain->ptr;
	chain->lptr = chain->ptr + chain->len - 1;
	chain->unuse_ptr = chain->ptr;

	fly_bllist_add_tail(&buffer->chain, &chain->blelem);
	buffer->chain_count++;
	return FLY_BUF_ADD_CHAIN_SUCCESS;
}

void fly_buffer_chain_release(fly_buffer_c *__c)
{
	fly_bllist_remove(&__c->blelem);

	__c->buffer->chain_count--;

	fly_pbfree(__c->buffer->pool, __c->ptr);
	fly_pbfree(__c->buffer->pool, __c);
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
		while(true){
			*c=fly_buffer_next_chain(*c);
			if (!*c)
				return NULL;
			if ((__len - (ssize_t) (*c)->len) <= 0)
				break;
			else
				__len -= (ssize_t) (*c)->len;
		}

		return (*c)->use_ptr+__len-1;
	}
}

int fly_update_buffer(fly_buffer_t *buf, size_t len)
{
	struct fly_buffer_chain *__l;
	ssize_t i;

	__l = fly_buffer_last_chain(buf);
	i = len;
	while ( (i - (ssize_t) __l->unuse_len) >= 0 ){
		i -= (ssize_t) __l->unuse_len;
		buf->use_len += __l->unuse_len;

		__l->use_len = __l->len;
		__l->unuse_len = 0;
		__l->status = FLY_BUF_FULL;
		__l->unuse_ptr = __l->lptr;
		switch (fly_buffer_add_chain(buf)){
		case FLY_BUF_ADD_CHAIN_SUCCESS:
			break;
		case FLY_BUF_ADD_CHAIN_LIMIT:
			return FLY_BUF_ADD_CHAIN_LIMIT;
		case FLY_BUF_ADD_CHAIN_ERROR:
			return FLY_BUF_ADD_CHAIN_ERROR;
		}

		__l = fly_buffer_last_chain(buf);
	}

	/* if i == 0, not use */
	if (i){
		buf->use_len += i;
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
		fly_buffer_c *__nc = fly_buffer_next_chain((*__c));
		*ptr = __nc->use_ptr;
		*__c = __nc;
	}else
		*ptr = *ptr+1;

	return res;
}

static inline bool __fly_bufp_end(fly_buffer_c *__c, fly_buf_p ptr)
{
	if (fly_is_chain_buffer_chain(__c->buffer, __c->blelem.next) && \
			ptr >= __c->lptr)
		return true;
	else
		return false;
}

#define FLY_BUFFER_STRSTR_AFTER			(1<<0)
__fly_static char *__fly_buffer_strstr(fly_buffer_c *__c, const char *str, int flag)
{
	fly_buf_p n;

	n = __c->use_ptr;
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
	fly_buffer_c *c = fly_buffer_first_chain(__b);
	fly_buf_p ptr;

	ptr = c->use_ptr;
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
			if (!fly_is_chain_term(c)){
				c = fly_buffer_next_chain(c);
				ptr = c->use_ptr;
			}else
				return 0;
		}else
			ptr++;
	}
}

int fly_buffer_memcmp(char *dist, char *src, fly_buffer_c *__c, size_t maxlen)
{
	char *sptr;
	sptr = src;

	while(sptr<(char *) __c->use_ptr || sptr>(char *) __c->lptr){
		__c=fly_buffer_next_chain(__c);
		if (fly_is_chain_term(__c))
			return FLY_BUFFER_MEMCMP_OVERFLOW;
	}

	while(maxlen--){
		if (*dist != *sptr){
			if (dist > sptr)
				return FLY_BUFFER_MEMCMP_BIG;
			else
				return FLY_BUFFER_MEMCMP_SMALL;
		}
		if (*sptr=='\0' && *dist=='\0')
			break;
		else if (__c->status == FLY_BUF_HALF && (char *) __c->unuse_ptr<=sptr)
			return FLY_BUFFER_MEMCMP_OVERFLOW;

		dist++;
		if (sptr >= (char *) __c->lptr){
			__c = fly_buffer_next_chain(__c);
			sptr = __c->use_ptr;
		}else
			sptr++;

	}

	return FLY_BUFFER_MEMCMP_EQUAL;
}

void fly_buffer_memcpy_all(char *dist, fly_buffer_t *__t)
{
	fly_buffer_memcpy(dist, fly_buffer_first_useptr(__t), fly_buffer_first_chain(__t), __t->use_len);
}

void fly_buffer_memncpy_all(char *dist, fly_buffer_t *__t, size_t n)
{
	fly_buffer_memcpy(dist, fly_buffer_first_useptr(__t), fly_buffer_first_chain(__t), n);
}

void fly_buffer_memcpy(char *dist, char *src, fly_buffer_c *__c, size_t len)
{
	char *sptr;
	sptr = src;

	while(sptr<(char *) __c->use_ptr || sptr>(char *) __c->lptr){
		__c = fly_buffer_next_chain(__c);
		if (fly_is_chain_term(__c))
			return;
	}

	while(len--){
		*dist++ = *sptr++;
		if (sptr > (char *) __c->lptr){
			__c = fly_buffer_next_chain(__c);
			sptr = __c->ptr;
		}
	}

	return;
}

fly_buffer_c *fly_buffer_chain_from_ptr(fly_buffer_t *buffer, fly_buf_p ptr)
{
	if (buffer->chain_count == 0)
		return NULL;

	struct fly_bllist *__b;
	fly_buffer_c *__c;
	fly_for_each_bllist(__b, &buffer->chain){
		__c = fly_buffer_chain(__b);
		if (__c->ptr<=ptr && __c->lptr>=ptr)
			return __c;
	}
	return NULL;
}

/*
 *	refresh ptr function.
 */
void fly_buffer_chain_refresh(fly_buffer_c *__c)
{
	__c->use_ptr = __c->ptr;
	__c->unuse_ptr = __c->use_ptr;
	__c->use_len = 0;
	__c->unuse_len = __c->len;
}
/*
 *	buffer chain release.
 *	@__c: start from this chain.
 *	@len: release bytes.
 */
void fly_buffer_chain_release_from_length(fly_buffer_c *__c, size_t len)
{
	if ((__c->lptr-__c->use_ptr+1) > (ssize_t) len){
		__c->use_ptr += len;
		__c->buffer->use_len -= len;
		return;
	}else{
		int delete_chain_count = 0;
		size_t total_delete_len = 0;
		fly_buffer_c *__n=__c;

		while((total_delete_len+(__n->lptr-__n->use_ptr+1)) <= len ){
			delete_chain_count++;
			total_delete_len+=(__n->lptr-__n->use_ptr+1);

			/* no next chain */
			if (fly_is_chain_term(__n))
				break;

			__n = fly_buffer_next_chain(__n);
		}

		__n = __c;
		while(delete_chain_count--){
			__n->buffer->use_len -= (__n->lptr-__n->use_ptr+1);

			fly_buffer_c *__tmp = fly_buffer_next_chain(__n);
			fly_buffer_chain_release(__n);
			__n = __tmp;
		}

		size_t left = len-total_delete_len;
		__n->use_ptr = __n->ptr+left;
		__n->buffer->use_len -= left;
	}
	return;
}

fly_buffer_c *fly_get_buf_chain(fly_buffer_t *buf, int i)
{
	fly_buffer_c *c;
	for (c=fly_buffer_first_chain(buf); i; i--)
		c = fly_buffer_next_chain(c);

	return c;
}

