#include "uri.h"
#include "mount.h"

bool fly_is_uri_index(fly_uri_t *uri)
{
	const char *index_path;
	char *ptr = uri->ptr;
	int i=0;

	while(fly_slash(*ptr++)){
		i++;
		/* only slash */
		if (uri->len <= (size_t) i)
			return true;
	}

	index_path = fly_index_path();
	if (strncmp(index_path, ptr, strlen(index_path)) == 0)
		return true;
	else
		return false;
}

