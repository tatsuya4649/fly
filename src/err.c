#include "err.h"

void fly_emerge_memory_zero(void);

__fly_static int __fly_err_logcont(fly_err_t *err, fly_logcont_t *lc);
static inline const char *__fly_level_str(fly_err_t *err);
static const char *fly_level_str(enum fly_error_level level);

static struct {
	pid_t pid;
	fly_context_t *ctx;
	struct flock lock;
} __fly_errsys;

void fly_errsys_init(fly_context_t *ctx)
{
	__fly_errsys.pid = getpid();
	__fly_errsys.ctx = ctx;
}

__attribute__ ((format (printf, 4, 5)))
fly_err_t *fly_err_init(fly_pool_t *pool, int __errno, enum fly_error_level level, const char *fmt, ...)
{
	fly_err_t *err;
	va_list ap;

	err = fly_pballoc(pool, sizeof(fly_err_t));

	memset(err->content, '\0', FLY_ERROR_CONTENT_SIZE);

	va_start(ap, fmt);
	snprintf(err->content, FLY_ERROR_CONTENT_SIZE, fmt, ap);
	va_end(ap);

	err->content_len = strlen(err->content);
	err->__errno = __errno;
	err->level	 = level;
	err->event   = NULL;
	err->pool	 = pool;

	fly_bllist_init(&err->blelem);
	return err;
}

void fly_err_release(struct fly_err *__e)
{
	fly_bllist_remove(&__e->blelem);
	fly_pbfree(__e->pool, __e);
}

static const char *fly_level_str(enum fly_error_level level)
{
	switch(level){
	case FLY_ERR_EMERG:
		return "EMERGE";
	case FLY_ERR_CRIT:
		return "CRIT";
	case FLY_ERR_ERR:
		return "ERROR";
	case FLY_ERR_ALERT:
		return "ALERT";
	case FLY_ERR_WARN:
		return "WARN";
	case FLY_ERR_NOTICE:
		return "NOTICE";
	case FLY_ERR_INFO:
		return "INFO";
	case FLY_ERR_DEBUG:
		return "DEBUG";
	default:
		return "UNKNOWN";
	}
}

static inline const char *__fly_level_str(fly_err_t *err)
{
#ifdef DEBUG
	assert(err);
#endif
	if (fly_unlikely_null(err))
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
	fly_logcont_setting(lc, FLY_ERROR_LOG_LENGTH);
	if (__fly_err_logcont(err, lc) == __FLY_ERROR_LOGCONTENT_ERROR)
		return -1;
	if (fly_log_now(&lc->when) == -1)
		return -1;

	e->event_data = (void *) lc;
	e->flag = 0;
	e->tflag = FLY_INFINITY;
	e->expired = false;
	e->available = false;
	FLY_EVENT_HANDLER(e, fly_log_event_handler);
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
	FLY_EVENT_HANDLER(e, fly_errlog_event_handler);
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
#ifdef HAVE_STRERRORNAME_NP
		strerrorname_np(errp->__errno),
#else
		"",
#endif
#ifdef HAVE_STRERRORDESC_NP
		strerrordesc_np(errp->__errno),
#else
		strerror(errp->__errno),
#endif
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
 * for emerge.
 */
__fly_static void __fly_write_to_log_emerg(const char *err_content, enum fly_error_level level, int __errno)
{
	fly_context_t *ctx;
	__fly_log_t *err, *notice;
	fly_logfile_t errfile, noticefile;
	void *__ptr;
	char *errc, *noticec;

	/* emergency pointer */
	__ptr = fly_emerge_memory;
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
#ifdef HAVE_STRERRORNAME_NP
		strerrorname_np(__errno),
#else
		"",
#endif
#ifdef HAVE_STRERRORDESC_NP
		strerrordesc_np(__errno)
#else
		strerror(__errno)
#endif
	);
	if (errfile != -1)
		write(errfile, errc, strlen(errc));
	if (ctx->log_stdout)
		fprintf(stdout, "%s", errc);
	if (ctx->log_stderr)
		fprintf(stderr, "%s", errc);

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
		"[%s] process(%d) was terminated by emergency error.\n",
		fly_level_str(level),
		__fly_errsys.pid
	);
	if (noticefile != -1)
		write(noticefile, noticec, strlen(noticec));
	if (ctx->log_stdout)
		fprintf(stdout, "%s", noticec);
	if (ctx->log_stderr)
		fprintf(stderr, "%s", noticec);

	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(noticefile, F_SETLKW, &__fly_errsys.lock);

	return;
}

__fly_static void __fly_write_to_log_err(const char *err_content, size_t len, enum fly_error_level level)
{
	fly_context_t *ctx;
	__fly_log_t *err, *notice;
	fly_logfile_t errfile, noticefile;
	char *errc, *noticec;
	size_t err_content_len, not_content_len;

	/* emergency pointer */
#define FLY_ERROR_PLACEHOLDER_SIZE			\
		strlen("[9999999999999] Occurred error. Worker process is gone. ")
	err_content_len = len+FLY_ERROR_PLACEHOLDER_SIZE;
	errc = fly_malloc(err_content_len);
	if (errc == NULL)
		return;

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
		err_content_len,
		"[%d] Occurred error. Worker process is gone. %s\n",
		__fly_errsys.pid,
		err_content
	);
	if (errfile != -1)
		write(errfile, errc, strlen(errc));
	if (ctx->log_stdout)
		fprintf(stdout, "%s", errc);
	if (ctx->log_stderr)
		fprintf(stderr, "%s", errc);

	fly_free(errc);
	/* release file lock */
	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(errfile, F_SETLKW, &__fly_errsys.lock);

	/* write notice log */
	__fly_errsys.lock.l_type = F_WRLCK;
	if (fcntl(noticefile, F_SETLKW, &__fly_errsys.lock) == -1)
		return;

	const char *format;

#define FLY_ERR_NOTICE_PLACEHOLDER_SIZE			\
		strlen("[NOTICE] process(99999999999) was terminated by critical error. ")
	not_content_len = FLY_ERR_NOTICE_PLACEHOLDER_SIZE;
	noticec = fly_malloc(not_content_len);
	switch(level){
	case FLY_ERR_CRIT:
		format = "[%s] process(%d) was terminated by critical error.\n";
		break;
	/* end worker process */
	case FLY_ERR_ERR:
		format = "[%s] process(%d) was terminated by error.\n";
		break;
	default:
		FLY_NOT_COME_HERE
	}
	snprintf(
		noticec,
		not_content_len,
		format,
		fly_level_str(level),
		__fly_errsys.pid
	);
	if (noticefile != -1)
		write(noticefile, noticec, strlen(noticec));
	if (ctx->log_stdout)
		fprintf(stdout, "%s", noticec);
	if (ctx->log_stderr)
		fprintf(stderr, "%s", noticec);

	fly_free(noticec);
	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(noticefile, F_SETLKW, &__fly_errsys.lock);

	return;
}

__fly_static void __fly_write_to_log_info(const char *content, size_t len, enum fly_error_level level)
{
	fly_context_t *ctx;
	__fly_log_t *notice;
	fly_logfile_t noticefile;
	char *noticec;
	const char *format;
	size_t notice_content_len;

	ctx = __fly_errsys.ctx;
	if (ctx == NULL)
		return;

	notice = ctx->log->notice;
	noticefile = notice->file;

	/* write notice log */
	__fly_errsys.lock.l_type = F_WRLCK;
	__fly_errsys.lock.l_whence = SEEK_END;
	__fly_errsys.lock.l_start = 0;
	__fly_errsys.lock.l_len = 0;
	if (fcntl(noticefile, F_SETLKW, &__fly_errsys.lock) == -1)
		return;

#define FLY_NOTICE_PLACEHOLDER_SIZE			\
		strlen("[ALERT] pid: (99999999999). ")
	notice_content_len = FLY_NOTICE_PLACEHOLDER_SIZE+len;
	noticec = fly_malloc(notice_content_len);
	format = "[%s] pid: (%d). %s\n";
	snprintf(
		noticec,
		notice_content_len,
		format,
		fly_level_str(level),
		__fly_errsys.pid,
		content
	);
	if (noticefile != -1)
		write(noticefile, noticec, strlen(noticec));
	if (ctx->log_stdout)
		fprintf(stdout, "%s", noticec);
	if (ctx->log_stderr)
		fprintf(stderr, "%s", noticec);

	fly_free(noticec);
	__fly_errsys.lock.l_type = F_UNLCK;
	fcntl(noticefile, F_SETLKW, &__fly_errsys.lock);

	return;
}
uint8_t fly_emerge_memory[FLY_EMERGE_MEMORY_SIZE];

void fly_emerge_memory_zero(void)
{
	memset(fly_emerge_memory, '\0', FLY_EMERGE_MEMORY_SIZE);
}

__noreturn __attribute__ ((format (printf, 2, 3)))
void fly_emergency_verror(int __errno, const char *format, ...)
{
	va_list va;
	char *err_content;

	err_content = (char *) fly_emerge_memory;
	fly_emerge_memory_zero();

	va_start(va, format);

	snprintf(err_content, FLY_EMERGE_MEMORY_SIZE, format, va);

	va_end(va);

	/* write error content in log */
	__fly_write_to_log_emerg(err_content, FLY_ERR_EMERG, __errno);
	exit((int) FLY_ERR_EMERG);
}

void fly_emergency_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_EMERG);
#endif
	/* write error content in log */
	__fly_write_to_log_emerg(err->content, FLY_ERR_EMERG, err->__errno);
	exit((int) FLY_ERR_EMERG);
}

__noreturn void fly_critical_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_CRIT);
#endif
	/* write error content in log */
	__fly_write_to_log_err(err->content, err->content_len, FLY_ERR_CRIT);
	exit((int) FLY_ERR_CRIT);
}

__noreturn __attribute__ ((format (printf, 2, 3)))
void fly_nomem_verror(__unused int __errno, const char *format, ...)
{
	va_list va;
	char *err_content;

	err_content = (char *) fly_emerge_memory;
	fly_emerge_memory_zero();

	va_start(va, format);
	snprintf(err_content, FLY_EMERGE_MEMORY_SIZE, format, va);
	va_end(va);

	/* write error content in log */
	__fly_write_to_log_err(err_content, strlen(err_content), FLY_ERR_ERR);
	exit((int) FLY_ERR_ERR);
}

__noreturn void fly_error_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_ERR);
#endif
	/* write error content in log */
	__fly_write_to_log_err(err->content, err->content_len, FLY_ERR_ERR);
	exit((int) FLY_ERR_ERR);
}

void fly_alert_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_ALERT);
#endif
	/* write error content in log */
	__fly_write_to_log_info(err->content, err->content_len, FLY_ERR_ALERT);
	fly_pbfree(err->pool, err);
	return;
}

void fly_warn_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_WARN);
#endif
	/* write error content in log */
	__fly_write_to_log_info(err->content, err->content_len, FLY_ERR_WARN);
	fly_pbfree(err->pool, err);
	return;
}

void fly_notice_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_NOTICE);
#endif
	/* write error content in log */
	__fly_write_to_log_info(err->content, err->content_len, FLY_ERR_NOTICE);
	fly_pbfree(err->pool, err);
	return;
}

void fly_info_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_NOTICE);
#endif
	/* write error content in log */
	__fly_write_to_log_info(err->content, err->content_len, FLY_ERR_NOTICE);
	fly_pbfree(err->pool, err);
	return;
}

void fly_debug_error(struct fly_err *err)
{
	assert(err != NULL);
#ifdef DEBUG
	assert(err->level == FLY_ERR_DEBUG);
#endif
	/* write error content in log */
	__fly_write_to_log_info(err->content, err->content_len, FLY_ERR_DEBUG);
	fly_pbfree(err->pool, err);
	return;
}
