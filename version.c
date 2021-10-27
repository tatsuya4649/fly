#include <string.h>
#include <stdio.h>
#include "util.h"
#include "version.h"

fly_http_version_t versions[] = {
	{"HTTP/2", "2", "h2", V2},
	{"HTTP/1.1", "1.1", "http/1.1", V1_1},
	{NULL},
};

int fly_version_str(char *buffer, fly_version_e version)
{
	for (fly_http_version_t *ver=versions; ver->full!=NULL; ver++){
		if (ver->type == version){
			strcpy(buffer, ver->full);
			return 0;
		}
	}
	/* not found valid http version */
	return -1;
}

fly_http_version_t *fly_match_version_from_type(enum fly_version_type type)
{
    for (fly_http_version_t *ver=versions; ver->full!=NULL; ver++){
		if (ver->type == type)
			return ver;
    }
    return NULL;
}

fly_http_version_t *fly_match_version_with_end(char *version, char end_of_version)
{
    for (fly_http_version_t *ver=versions; ver->full!=NULL; ver++){
		char *ptr = ver->number;
		while(*version++ == *ptr++){
			if (*version == end_of_version)
				return ver;
		}
    }
    return NULL;
}

fly_http_version_t *fly_match_version(char *version)
{
    /* version name should be upper */
    for (char *n=version; *n!='\0'; n++){
        *n = toupper((int) *n);
	}

    for (fly_http_version_t *ver=versions; ver->full!=NULL; ver++){
        if (strcmp(version, ver->full) == 0)
            return ver;
    }
    return NULL;
}

__fly_static int __fly_version_alpn_cmp(const unsigned char *dist, const unsigned char *version, unsigned int len)
{
	unsigned int total=0;
	while(total++ < len)
		if (*dist++ != *version++)
			return -1;
	return 0;
}

fly_http_version_t *fly_match_version_from_alpn(const unsigned char *version, unsigned int len)
{
    for (fly_http_version_t *ver=versions; ver->full!=NULL; ver++){
		if (__fly_version_alpn_cmp((unsigned char *) ver->alpn, version, len) == 0)
			return ver;
    }
    return NULL;
}

fly_http_version_t *fly_default_http_version(void)
{
	for (fly_http_version_t *__v=versions; __v->full; __v++){
		if (__v->type == FLY_HTTP_DEFAULT_VERSION)
			return __v;
		else
			continue;
	}
	FLY_NOT_COME_HERE
	return NULL;
}
