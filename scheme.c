#include <string.h>
#include "scheme.h"

static struct fly_scheme schemes[] = {
	FLY_SCHEME_SET(http),
	FLY_SCHEME_SET(https),
	{-1, NULL},
};

int is_fly_scheme(char **c, char end_of_char)
{
	struct fly_scheme *__s;

	for (__s=schemes; __s->type>0; __s++){
		while( *((*c)++) == *__s->name++)
			if (**c == end_of_char)
				return 1;
	}
	return 0;
}

fly_scheme_t *fly_match_scheme_name(char *name)
{
	fly_scheme_t *s;
    for (s=schemes; s->name; s++){
        if (strncmp(name, s->name, strlen(s->name)) == 0){
            return s;
		}
    }
    return NULL;
}
