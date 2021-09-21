#ifndef _FLY_STRING_H
#define _FLY_STRING_H

struct fly_string{
	char *str;
	size_t len;
};
typedef struct fly_string fly_string_t;
#define fly_string_from_char(__s, c)	\
	do{									\
		(__s).str = (c);				\
		(__s).len = strlen((c));		\
	} while(0)
#endif
