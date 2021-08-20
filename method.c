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
