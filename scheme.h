#ifndef _SCHEME_H
#define _SCHEME_H

#include <stddef.h>

enum fly_scheme_e{
	fly_http,
	fly_https,
};

struct fly_scheme{
	enum fly_scheme_e type;
	char *name;
};

#define FLY_SCHEME_SET(s)		{ fly_ ## s, #s }
extern struct fly_scheme schems[];
int is_fly_scheme(char **c, char end_of_char);
#endif
