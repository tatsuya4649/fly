#include "method.h"

fly_http_method_t methods[] = {
	{"GET", GET},
	{"HEAD", HEAD},
	{"POST", POST},
	{"PUT", PUT},
	{"DELETE", DELETE},
	{"CONNECT", CONNECT},
	{"OPTIONS", OPTIONS},
	{"TRACE", TRACE},
	{"PATCH", PATCH},
	{NULL}
};

fly_http_method_t *fly_match_method_name(char *method_name)
{
	if (method_name == NULL)
		return NULL;

	fly_http_method_t *m;
    for (m=methods; m->name!=NULL; m++){
        if (strcmp(method_name, m->name) == 0){
            return m;
		}
    }
    return NULL;
}

fly_http_method_t *fly_match_method_name_with_end(char *method_name, char end_of_char)
{
	if (method_name == NULL)
		return NULL;

	fly_http_method_t *m;
	char *__ptr;
    for (m=methods; m->name!=NULL; m++){
		char *ptr;

		ptr = m->name;
		__ptr = method_name;
		while(*__ptr++ == *ptr++){
			if (*__ptr == end_of_char)
				return m;
		}
    }
    return NULL;
}

fly_http_method_t *fly_match_method_type(fly_method_e method)
{
	fly_http_method_t *m;
    for (m=methods; m->name!=NULL; m++){
        if (m->type == method)
            return m;
    }
    return NULL;
}

fly_method_e *fly_match_method_name_e(char *name)
{
	fly_http_method_t *m;
    for (m=methods; m->name!=NULL; m++){
        if (strcmp(name, m->name) == 0)
            return &m->type;
    }
    return NULL;
}
