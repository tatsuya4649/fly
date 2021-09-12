#ifndef _LOG_H
#define _LOG_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/time.h>
#include "util.h"
#include "fs.h"
#include "alloc.h"
#include "err.h"

#define FLY_ACCESLOG_ENV				"FYL_ACCESLOG_ENV"
#define FLY_ERRORLOG_ENV				"FYL_ERRORLOG_ENV"
#define FLY_NOTICLOG_ENV				"FYL_NOTICLOG_ENV"
#define FLY_DEFAULT_LOGDIR					"./log/"
#define FLY_ACCESLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_access.log")
#define FLY_ERRORLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_error.log")
#define FLY_NOTICLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_notice.log")
typedef char				fly_path_t;
typedef char				fly_logc_t;
typedef int					fly_logfile_t;
typedef struct __fly_log	__fly_log_t;
typedef struct fly_log		fly_log_t;

#define FLY_LOG_POOL_SIZE					10
extern fly_pool_t *fly_log_pool;
struct fly_log{
	__fly_log_t *access;
	__fly_log_t *error;
	__fly_log_t *notice;
	fly_pool_t *pool;
};

enum fly_log_type{
	ACCESS,
	ERROR,
	NOTICE,
};
typedef enum fly_log_type fly_log_e;

#define FLY_LOG_PLACE_SIZE					100
#define FLY_LOG_BODY_SIZE					1000

struct __fly_log{
	fly_logfile_t file;
	fly_path_t log_path[FLY_PATH_MAX];
};

fly_log_t *fly_log_init(void);
int fly_log_release(fly_log_t *log);

#include "ftime.h"
struct fly_logcont{
	/* log content */
	fly_logc_t		*content;
	/* length of content(not including end of \0). */
	size_t			contlen;
	/* log type */
	fly_log_e		type;
	fly_time_t		when;

	fly_log_t		*log;

	struct flock	lock;
};
typedef struct fly_logcont fly_logcont_t;

#include "event.h"
fly_logcont_t *fly_logcont_init(fly_log_t *log, fly_log_e type);
int fly_logcont_setting(fly_logcont_t *lc, size_t content_length);
#define fly_log_from_manager(m)		((m)->ctx->log)
#define fly_log_from_event(e)		(fly_log_from_manager((e)->manager))
#define FLY_LOGFILE_MODE			(S_IRUSR|S_IWUSR)
#define FLY_LOGMAX_SUFFIX			"..."
#define fly_maxlog_length(len)		((len) - strlen(FLY_LOGMAX_SUFFIX))
#define fly_maxlog_suffix_point(ptr, len)	((ptr) + (len) - 1 - strlen(FLY_LOGMAX_SUFFIX))

enum fly_log_fase{
	EFLY_LOG_FASE_INIT,
	EFLY_LOG_FASE_WRITE,
	EFLY_LOG_FASE_END,
};
enum fly_log_state{
	EFLY_LOG_STATE_WAIT,
	EFLY_LOG_STATE_ACTIVE,
};
typedef enum fly_log_fase fly_log_fase_e;
typedef enum fly_log_state fly_log_state_e;

int fly_log_event_handler(fly_event_t *e);
int fly_log_now(fly_time_t *t);

#endif
