#include <string.h>
#include "version.h"

http_version_t versions[] = {
	{"HTTP/1.1", "1.1", V1_1},
	{NULL},
};

int fly_version_str(char *buffer, fly_version_e version)
{
	for (http_version_t *ver=versions; ver->full!=NULL; ver++){
		if (ver->type == version){
			strcpy(buffer, ver->full);
			return 0;
		}
	}
	/* not found valid http version */
	return -1;
}
