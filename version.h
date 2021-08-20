#ifndef _VERSION_H
#define _VERSION_H

#include <stddef.h>

enum version_type{
	V1_1
};
typedef enum version_type fly_version_e;

struct http_version{
	char *full;
	char *number;
	enum version_type type;
};
typedef struct http_version http_version_t;

extern http_version_t versions[];

#define FLY_VERSION_MAXLEN			10
int fly_version_str(char *buffer, fly_version_e version);

#endif
