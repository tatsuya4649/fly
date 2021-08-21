#include "method.h"

http_method methods[] = {
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

http_method *fly_match_method(char *method_name)
{
    /* method name should be lower */
    for (char *n=method_name; *n!='\0'; n++)
        *n = tolower((int) *n);

    for (http_method *type=methods; type->name!=NULL; type++){
        if (strcmp(method_name, type->name) == 0)
            return type;
    }
    return NULL;
}


