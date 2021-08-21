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

http_version_t *fly_match_version(char *version)
{
    /* version name should be upper */
    for (char *n=version; *n!='\0'; n++)
        *n = toupper((int) *n);

    for (http_version_t *ver=versions; ver->full!=NULL; ver++){
        if (strcmp(version, ver->full) == 0)
            return ver;
    }
    return NULL;
}

