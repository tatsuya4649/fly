#include "ftime.h"


int fly_logtime(char *buffer, int bufsize)
{
	return snprintf(buffer, bufsize, "%ld", time(NULL));
}
