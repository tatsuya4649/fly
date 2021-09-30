#ifndef _VERSION_H
#define _VERSION_H

#include <stddef.h>
#include <ctype.h>

enum version_type{
	V2,
	V1_1
};
typedef enum version_type fly_version_e;

struct fly_http_version{
	char *full;
	char *number;
	const char *alpn;
	enum version_type type;
};
typedef struct fly_http_version fly_http_version_t;

extern fly_http_version_t versions[];

#define FLY_VERSION_MAXLEN			10
int fly_version_str(char *buffer, fly_version_e version);
fly_http_version_t *fly_match_version(char *version);
fly_http_version_t *fly_match_version_with_end(char *version, char end_of_version);
fly_http_version_t *fly_match_version_from_alpn(const unsigned char *version, unsigned int len);

#endif
