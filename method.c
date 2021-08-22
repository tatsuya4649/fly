#include "method.h"

fly_http_method_t methods[] = {
	{"get", GET},
	{"head", HEAD},
	{"post", POST},
	{"put", PUT},
	{"delete", DELETE},
	{"connect", CONNECT},
	{"options", OPTIONS},
	{"trace", TRACE},
	{"patch", PATCH},
	{NULL}
};

fly_http_method_t *fly_match_method_name(char *method_name)
{
    /* method name should be lower */
    for (char *n=method_name; *n!='\0'; n++){
        *n = (char) tolower((int) *n);
	}

	fly_http_method_t *m;
    for (m=methods; m->name!=NULL; m++){
        if (strcmp(method_name, m->name) == 0){
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
