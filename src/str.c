
#include "str.h"

char *fly_strchr(fly_string_t *__s, char c)
{
	if (!__s || !__s->len)
		return NULL;

	for (int i=0; i<__s->len; i++){
		if (__s->str[i] == c)
			return &__s->str[i];
	}
	return NULL;
}

int fly_str_alloc(fly_pool_t *pool, fly_str_t *__str, size_t len)
{
	__str->ptr = fly_pballoc(pool, sizeof(char)*(len+1));
	if (fly_unlikely_null(__str->ptr))
		return -1;

	__str->len = len;
	__str->pool = pool;
	return 0;
}

void fly_str_dealloc(fly_str_t *__str)
{
	fly_pbfree(__str->pool, __str->ptr);
	fly_str_init(__str);
}

void fly_str_copy(fly_str_t *str, char *__p, size_t len)
{
#ifdef DEBUG
	assert(str != NULL);
	assert(str->ptr != NULL);
#endif
#define FLY_STR_LEN_MAX(l1, l2)					\
		((l1) > (l2) ? l1 : l2)
	memcpy(str->ptr, __p, FLY_STR_LEN_MAX(len, str->len));
}
