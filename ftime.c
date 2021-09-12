#include "ftime.h"

int fly_logtime(char *buffer, int bufsize, fly_time_t *t)
{
	return snprintf(buffer, bufsize, "%ld", (long) t->tv_sec);
}
