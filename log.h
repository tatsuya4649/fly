#ifndef _LOG_H
#define _LOG_H

#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "ftime.h"
#include "util.h"
#include "fs.h"
#include "alloc.h"

#define FLY_ACCESLOG_ENV				"FYL_ACCESLOG_ENV"
#define FLY_ERRORLOG_ENV				"FYL_ERRORLOG_ENV"
#define FLY_NOTICLOG_ENV				"FYL_NOTICLOG_ENV"
#define FLY_DEFAULT_LOGDIR					"./log/"
#define FLY_ACCESLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_access.log")
#define FLY_ERRORLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_error.log")
#define FLY_NOTICLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_notice.log")
typedef char				fly_path_t;
typedef FILE				fly_log_fp;
typedef struct __fly_log	__fly_log_t;
typedef struct fly_log		fly_log_t;

#define FLY_LOG_POOL_SIZE					10
extern fly_pool_t *fly_log_pool;
struct fly_log{
	__fly_log_t *access;
	__fly_log_t *error;
	__fly_log_t *notice;
	fly_pool_t **pool;
};

#define FLY_LOG_PLACE_SIZE					100
#define FLY_LOG_BODY_SIZE					1000

struct __fly_log{
	fly_log_fp *fp;
	fly_path_t log_path[FLY_PATH_MAX];
};

fly_log_t *fly_log_init(void);
int fly_log_release(fly_log_t *log);

#endif
