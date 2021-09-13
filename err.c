#include "err.h"


__fly_static fly_pool_t *fly_err_pool = NULL;
/*
 * pointer for emergency (size -> FLY_RERRPTR_FOR_EMERGE_SIZE macro )
 *
 */
__fly_static void *fly_errptr_for_emerge = NULL;
__fly_static int fly_err_pool_init(void);
__fly_static int __fly_err_logcont(fly_err_t *err, fly_logcont_t *lc);
__fly_static inline const char *__fly_level_str(fly_err_t *err);

__fly_static struct {
	pid_t pid;
	fly_context_t *ctx;
	fly_pool_t *pool;
	struct flock lock;
} __fly_errsys;

int fly_errsys_init(fly_context_t *ctx)
{
	if (fly_err_pool == NULL)
		fly_err_pool = fly_create_pool(FLY_ERR_POOL_SIZE);

	if (fly_err_pool == NULL)
		return -1;

	fly_errptr_for_emerge = fly_pballoc(fly_err_pool, FLY_ERRPTR_FOR_EMERGE_SIZE);
	if (fly_errptr_for_emerge == NULL)
		return -1;

	__fly_errsys.pid = getpid();
	__fly_errsys.pool = fly_err_pool;
	__fly_errsys.ctx = ctx;

	return 0;
}

__fly_static int fly_err_pool_init(void)
{
	if ((fly_err_pool == NULL || fly_errptr_for_emerge == NULL))
		return -1;

	if (fly_err_pool == NULL)
		return -1;
	else
		return 0;
}

fly_err_t *fly_err_init(fly_errc_t *content, int __errno, int level)
{
	if (fly_err_pool_init() == -1)
		return NULL;

	fly_err_t *err;
	err = fly_pballoc(fly_err_pool, sizeof(fly_err_t));
	if (err == NULL)
		return NULL;

	err->content = content;
	err->__errno = __errno;
	err->level = level;
	err->event   = NULL;
	err->pool	 = fly_err_pool;
	return err;
}

__fly_static inline const char *__fly_level_str(fly_err_t *err)
{
	if (err == NULL)
		return NULL;
	switch(err->level){
	case FLY_ERR_EMERG:
		return "EMERGENCY";
	case FLY_ERR_ALERT:
		return "ALERT";
	case FLY_ERR_CRIT:
		return "CRITICAL";
	case FLY_ERR_ERR:
		return "ERROR";
	case FLY_ERR_WARN:
		return "WARNING";
	case FLY_ERR_NOTICE:
		return "NOTICE";
	case FLY_ERR_INFO:
		return "INFO";
	case FLY_ERR_DEBUG:
		return "DEBUG";
	default:
		return NULL;
	}
}

__fly_static int __fly_err_logcont(__unused fly_err_t *err, fly_logcont_t *lc)
{
#define __FLY_ERROR_LOGCONTENT_SUCCESS			1
#define __FLY_ERROR_LOGCONTENT_ERROR			-1
#define __FLY_ERROR_LOGCONTENT_OVERFLOW			0
	int res;

	res = snprintf(
		lc->content,
		lc->contlen,
		"%s[%s]: %s\n",
		__fly_level_str(err),
		err->__errno != FLY_NULL_ERRNO ? strerror(err->__errno) : FLY_NULL_ERRNO_DESC,
		err->content
	);
	if (res >= (int) fly_maxlog_length(lc->contlen)){
		memcpy(fly_maxlog_suffix_point(lc->content,lc->contlen), FLY_LOGMAX_SUFFIX, strlen(FLY_LOGMAX_SUFFIX));
		return __FLY_ERROR_LOGCONTENT_OVERFLOW;
	}
	lc->contlen = res;
	return __FLY_ERROR_LOGCONTENT_SUCCESS;
}

int fly_errlog_event_handler(fly_event_t *e)
{
	__unused fly_err_t *err;
	__unused fly_logcont_t *lc;

	lc = fly_logcont_init(fly_log_from_event(e), FLY_LOG_ERROR);

	err = (fly_err_t *) e->event_data;
	if (lc == NULL)
		return -1;
	if (fly_logcont_setting(lc, FLY_ERROR_LOG_LENGTH) == -1)
		return -1;
	if (__fly_err_logcont(err, lc) == __FLY_ERROR_LOGCONTENT_ERROR)
		return -1;
	if (fly_log_now(&lc->when) == -1)
		return -1;

	e->event_data = (void *) lc;
	e->flag = 0;
	e->tflag = FLY_INFINITY;
	e->expired = false;
	e->available = false;
	e->handler = fly_log_event_handler;
	fly_event_regular(e);

	return fly_event_register(e);
}

int fly_errlog_event(fly_event_manager_t *manager, fly_err_t *err)
{
	fly_event_t *e;

	if (!manager || !err)
		return -1;

	e = fly_event_init(manager);
	if (e == NULL)
		return -1;

	e->manager = manager;
	e->fd = fly_errlogfile_from_manager(manager);
	e->read_or_write = FLY_READ;
	e->tflag = 0;
	e->flag = 0;
	e->eflag = 0;
	e->handler = fly_errlog_event_handler;
	e->event_data = (void *) err;
	e->available = false;
	e->expired = false;
	fly_time_zero(e->timeout);
	fly_event_regular(e);

	return fly_event_register(e);
}

#include <string.h>
__unused __fly_static void __fly_printf_error(fly_errp_t *errp, FILE *fp)
{
	fprintf(
		fp,
		"  [%s (%s)]: %s\n",
		strerrorname_np(errp->__errno),
		strerrordesc_np(errp->__errno),
		errp->content
	);
}

void fly_stdout_error(fly_errp_t *errp){
	__fly_printf_error((errp), stdout);
}
void fly_stderr_error(fly_errp_t *errp){
	__fly_printf_error((errp), stderr);
}

/*
 * for emergency error. noreturn function.
 */

__fly_static void __fly_write_to_log_emerge(fly_errc_t *err_content, enum fly_emergency_status status, int __errno)
{
	fly_context_t *ctx;
	__fly_log_t *err, *notice;
	fly_logfile_t errfile, noticefile;
	void *__ptr;
	char *errc, *noticec;

	/* emergency pointer */
	__ptr = fly_errptr_for_emerge;
	if (__ptr == NULL)
		return;
	errc = (char *) __ptr;
	noticec = __ptr + FLY_EMERGENCY_LOG_LENGTH;
	ctx = __fly_errsys.ctx;
	if (ctx == NULL)
		return;

	err = ctx->log->error;
	notice = ctx->log->notice;
	errfile = err->file;
	noticefile = notice->file;


	/* get file lock */
	__fly_errsys.lock.l_type = F_WRLCK;
	__fly_errsys.lock.l_whence = SEEK_END;
	__fly_errsys.lock.l_start = 0;
	__fly_errsys.lock.l_len = 0;

	/* write error log */
	if (fcntl(errfile, F_SETLKW, &__fly_errsys.lock) == -1)
		return;

	snprintf(
		errc,
		FLY_EMERGENCY_LOG_LENGTH,
		"[%d] Emergency Error. Worker Process is gone. (%s) (%s: %s)\n",
		__fly_errsys.pid,
		err_content,
		strerrorname_np(__errno),
		strerrordesc_np(__errno)
	);
	write(errfile, errc, strlen(errc));

	/* release file lock */
	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(errfile, F_SETLKW, &__fly_errsys.lock);

	/* write notice log */
	__fly_errsys.lock.l_type = F_WRLCK;
	if (fcntl(noticefile, F_SETLKW, &__fly_errsys.lock) == -1)
		return;

	snprintf(
		noticec,
		FLY_EMERGENCY_LOG_LENGTH,
		"process[%d] is end by emergency error (%d)\n",
		__fly_errsys.pid,
		status
	);
	write(noticefile, noticec, strlen(noticec));

	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(noticefile, F_SETLKW, &__fly_errsys.lock);

	return;
}

__noreturn __attribute__ ((format (printf, 3, 4)))
void fly_emergency_error(enum fly_emergency_status end_status, int __errno, const char *format, ...)
{
#define __FLY_EMERGENCY_ERROR_CONTENT_MAX		100
	va_list va;
	char err_content[__FLY_EMERGENCY_ERROR_CONTENT_MAX];
	va_start(va, format);

	snprintf(err_content, __FLY_EMERGENCY_ERROR_CONTENT_MAX, format, va);

	va_end(va);
	/* write error content in log */
	__fly_write_to_log_emerge(err_content, end_status, __errno);
	exit((int) end_status);
}
