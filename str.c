
#include "string.h"

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

char *fly_cptr_from_string
