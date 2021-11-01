#ifndef _LOG_H
#define _LOG_H

#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <fcntl.h>
#include <sys/time.h>
#include "util.h"
#include "mount.h"
#include "alloc.h"

#define FLY_STDOUT_ENV(name)			("FLY_STDOUT_" # name)
#define FLY_STDERR_ENV(name)			("FLY_STDERR_" # name)
#define FLY_ACCESLOG_ENV				"FYL_ACCESLOG_ENV"
#define FLY_ERRORLOG_ENV				"FYL_ERRORLOG_ENV"
#define FLY_NOTICLOG_ENV				"FYL_NOTICLOG_ENV"
#define FLY_DEFAULT_LOGDIR				(fly_path_t *) 	"./log/"

#define FLY_ERRORLOG_FILENAME					"fly_error.log"
#define FLY_ACCESLOG_FILENAME					"fly_access.log"
#define FLY_NOTICLOG_FILENAME					"fly_notice.log"
#define FLY_ACCESLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_access.log")
#define FLY_ERRORLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_error.log")
#define FLY_NOTICLOG_DEFAULT			(FLY_DEFAULT_LOGDIR "fly_notice.log")
#define FLY_LOG_PATH					"FLY_LOG_PATH"
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
	FLY_LOG_ACCESS,
	FLY_LOG_ERROR,
	FLY_LOG_NOTICE,
};
typedef enum fly_log_type fly_log_e;

#define FLY_LOG_PLACE_SIZE					100
#define FLY_LOG_BODY_SIZE					1000

struct __fly_log{
	fly_logfile_t	file;
	fly_path_t		log_path[FLY_PATH_MAX];
#define __FLY_LOGFILE_INIT_STDOUT			1 << 0
#define __FLY_LOGFILE_INIT_STDERR			1 << 1
	int				flag;
	fly_bit_t		tty: 1;
};

fly_log_t *fly_log_init(fly_context_t *ctx);
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
	__fly_log_t		*__log;

	struct flock	lock;
};
typedef struct fly_logcont fly_logcont_t;

#include "event.h"
#include "context.h"
/*
 *  fly_logcont is auto release resource.
 *  if write log, this resource is released.
 */
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

/*
 *  direct log (non event log).
*/
#define FLY_NOTICE_DIRECT_LOG_MAXLENGTH			200
#define FLY_NOTICE_DIRECT_LOG					fly_notice_direct_log
void fly_notice_direct_log(fly_log_t *log, const char *fmt, ...);
int fly_log_event_register(fly_event_manager_t *manager, struct fly_logcont *lc);

const char *fly_log_path(void);

#define FLY_LOG_STDOUT						"FLY_LOG_STDOUT"
#define FLY_LOG_STDERR						"FLY_LOG_STDERR"
bool fly_log_stdout(void);
bool fly_log_stderr(void);

#endif
