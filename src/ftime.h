#ifndef _FTIME_H
#define _FTIME_H

#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif
#include <sys/time.h>
#include <time.h>
#include <stdbool.h>
#include <stdio.h>
#include "util.h"

#define FLY_TIME_MAX			50
#define FLY_TFORMAT_LEN			30
typedef struct timeval fly_time_t;

int fly_logtime(char *buffer, int bufsize, fly_time_t *t);
int fly_imt_fixdate(char *buf, size_t buflen, time_t *time);

#define FLY_IMT_FIXDATE_FORMAT			("%a, %d %b %Y %H:%M:%S GMT")
int fly_cmp_imt_fixdate(char *t1, __fly_unused size_t t1_len, char *t2, __fly_unused size_t t2_len);

#endif
