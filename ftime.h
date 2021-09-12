#ifndef _FTIME_H
#define _FTIME_H

#include <sys/time.h>
#include <time.h>
#include <stdio.h>

#define FLY_TIME_MAX			50
typedef struct timeval fly_time_t;

int fly_logtime(char *buffer, int bufsize, fly_time_t *t);

#endif
