#include <string.h>
#include "log.h"
#include <errno.h>
#include "err.h"
#include "conf.h"

__fly_static __fly_log_t *__fly_log_from_type(fly_log_t *lt, fly_log_e type);
__fly_static int __fly_log_write_logcont(fly_logcont_t *lc);
__fly_static int __fly_log_write(fly_logfile_t file, fly_logcont_t *lc);
__fly_static int __fly_placeholder(char *plh, size_t plh_size, fly_time_t t);

__fly_static int __fly_make_logdir(fly_path_t *dir, size_t dirsize);

__fly_static int __fly_error_log_path(char *log_path_buf, size_t buflen)
{
	const char *path;
	char *__lp = log_path_buf;

	path = fly_log_path();

	memset(log_path_buf, '\0', buflen);
	if (path != NULL){
		memcpy(log_path_buf, path, strlen(path));
		log_path_buf += strlen(path);
		memcpy(log_path_buf, "/", 1);
		log_path_buf += 1;
		memcpy(log_path_buf, FLY_ERRORLOG_FILENAME, strlen(FLY_ERRORLOG_FILENAME));
		if (__lp+buflen <= log_path_buf)
			return -1;
	}else
		memcpy(log_path_buf, FLY_ERRORLOG_DEFAULT, strlen(FLY_ERRORLOG_DEFAULT));

	return 0;
}

__fly_static int __fly_access_log_path(char *log_path_buf, size_t buflen)
{
	const char *path;
	char *__lp = log_path_buf;

	path = fly_log_path();
	memset(log_path_buf, '\0', buflen);
	if (path != NULL){
		memcpy(log_path_buf, path, strlen(path));
		log_path_buf += strlen(path);
		memcpy(log_path_buf, "/", 1);
		log_path_buf += 1;
		memcpy(log_path_buf, FLY_ACCESLOG_FILENAME, strlen(FLY_ACCESLOG_FILENAME));

		if (__lp+buflen <= log_path_buf)
			return -1;
	}else
		memcpy(log_path_buf, FLY_ACCESLOG_DEFAULT, strlen(FLY_ACCESLOG_DEFAULT));
	return 0;
}

__fly_static int __fly_notice_log_path(char *log_path_buf, size_t buflen)
{
	const char *path;
	char *__lp = log_path_buf;

	path = fly_log_path();
	memset(log_path_buf, '\0', buflen);
	if (path != NULL){
		memcpy(log_path_buf, path, strlen(path));
		log_path_buf += strlen(path);
		memcpy(log_path_buf, "/", 1);
		log_path_buf += 1;
		memcpy(log_path_buf, FLY_NOTICLOG_FILENAME, strlen(FLY_NOTICLOG_FILENAME));

		if (__lp+buflen <= log_path_buf)
			return -1;
	}else
		memcpy(log_path_buf, FLY_NOTICLOG_DEFAULT, strlen(FLY_NOTICLOG_DEFAULT));
	return 0;
}

#define __FLY_LOGFILE_INIT_STDOUT			1 << 0
#define __FLY_LOGFILE_INIT_STDERR			1 << 1
__fly_static __fly_log_t *__fly_logfile_init(fly_pool_t *pool, const fly_path_t *fly_log_path, int flag)
{
	__fly_log_t *lt;
	fly_logfile_t lfile;

	lt = fly_pballoc(pool, sizeof(__fly_log_t));
	if (lt == NULL)
		return NULL;

	if (fly_log_path != NULL){
		if (__fly_make_logdir((fly_path_t *) fly_log_path, strlen(fly_log_path)) < 0)
			return NULL;

		lfile = open(fly_log_path, O_RDWR|O_CREAT, FLY_LOGFILE_MODE);
		if (lfile == -1)
			return NULL;

		lt->file = lfile;
	}else
		lt->file = -1;

	if (lt->file != -1 && isatty(lt->file))
		lt->tty = true;
	else
		lt->tty = false;
	lt->flag = flag;
	strcpy(lt->log_path, fly_log_path);
	return lt;
}

#define __fly_log_path(name, buf, buflen)		\
			__fly_ ## name ## _log_path((buf), (buflen))

__fly_static int __fly_make_logdir(fly_path_t *dir, size_t dirsize)
{
	size_t n = 0;
	fly_path_t *ptr = dir;
	fly_path_t path[FLY_PATH_MAX];
	bool only_word = true;

	if (dirsize > FLY_PATH_MAX)
		return FLY_EBUFLEN;

	while(n<=dirsize){
		if (*ptr == '/'){
			int path_length = (ptr - dir);

			if (FLY_PATH_MAX < path_length)
				return FLY_EBUFLEN;

			strncpy(path, dir, path_length);
			path[path_length] = '\0';

			if (fly_isdir(path) <= 0)
				return FLY_ENFOUNDDIR;

			only_word = false;
		}

		if (*ptr == '\0'){
			if (only_word && fly_isdir(dir) <= 0)
				return FLY_ENFOUNDDIR;
			return FLY_SUCCESS;
		}

		ptr++;
		n++;
	}
	return FLY_EBUFLEN;
}

static inline int __fly_log_stdout()
{
	return fly_log_stdout() ? __FLY_LOGFILE_INIT_STDOUT: 0;
}

static inline int __fly_log_stderr()
{
	return fly_log_stderr() ? __FLY_LOGFILE_INIT_STDERR: 0;
}

fly_log_t *fly_log_init(fly_context_t *ctx)
{
	char log_path_buf[FLY_PATH_MAX];
	fly_log_t *lt;
	__fly_log_t *alp, *elp, *nlp;

	lt = fly_pballoc(ctx->pool, sizeof(fly_log_t));
	if (!lt)
		goto error;

	if (__fly_log_path(access, log_path_buf, FLY_PATH_MAX) == -1)
		return NULL;
	alp = __fly_logfile_init(
		ctx->pool,
		log_path_buf,
		__fly_log_stdout() | \
		__fly_log_stderr()
	);

	if (__fly_log_path(error, log_path_buf, FLY_PATH_MAX) == -1)
		return NULL;
	elp = __fly_logfile_init(
		ctx->pool,
		log_path_buf,
		__fly_log_stdout() | \
		__fly_log_stderr()
	);

	if (__fly_log_path(notice, log_path_buf, FLY_PATH_MAX) == -1)
		return NULL;
	nlp = __fly_logfile_init(
		ctx->pool,
		log_path_buf,
		__fly_log_stdout() | \
		__fly_log_stderr()
	);
	if (!alp || !elp || !nlp)
		goto error;

	lt->access = alp;
	lt->error = elp;
	lt->notice = nlp;
	lt->pool = ctx->pool;
	return lt;
error:
	return NULL;
}
int fly_log_release(fly_log_t *lt)
{
	if (!lt || !lt->pool)
			return -1;
	if (close(lt->access->file) == -1)
		return -1;
	if (close(lt->error->file) == -1)
		return -1;
	if (close(lt->notice->file) == -1)
		return -1;

	fly_pbfree(lt->pool, lt);
	return 0;
}

#include "ftime.h"
__fly_static int __fly_placeholder(char *plh, size_t plh_size, fly_time_t t)
{
	char ftime[FLY_TIME_MAX];

	if (fly_logtime(ftime, FLY_TIME_MAX, &t) == -1)
		return -1;
	return snprintf(plh, plh_size, "%s (%d): ", ftime, getpid());
}

__fly_static int __fly_log_lock(fly_logfile_t file, struct flock *lock)
{
#define FLY_LOG_LOCK_SUCCESS	1
#define FLY_LOG_LOCK_WAIT		0
#define FLY_LOG_LOCK_ERROR		-1
	int res;

	lock->l_type = F_WRLCK;
	lock->l_whence = SEEK_END;
	lock->l_start = 0;
	/* lock from end of log file */
	lock->l_len = 0;

	res = fcntl(file, F_SETLK, lock);

	if (res == -1){
		if (errno == EAGAIN || errno == EACCES)
			return FLY_LOG_LOCK_WAIT;
		else
			return FLY_LOG_LOCK_ERROR;
	}
	return FLY_LOG_LOCK_SUCCESS;
}

__fly_static int __fly_log_unlock(fly_logfile_t file, struct flock *lock)
{
	return fcntl(file, F_SETLK, lock);
}

__fly_static int __fly_write(fly_logfile_t file, size_t length, fly_logc_t *content)
{
	size_t total = 0;

	/* move to end of file */
	if (!isatty(file) && (lseek(file, 0, SEEK_END) == -1))
		return -1;
	while(true){
#ifndef FLY_LOG_WRITE_SIZE
#define FLY_LOG_WRITE_SIZE		sizeof(fly_logc_t)
#endif
		int now_pos=0, n, write_length;
		write_length = length - total;
		if (!isatty(file) && ((now_pos = lseek(file, 0, SEEK_CUR)) == -1))
				goto error;

		n = write(file, content, FLY_LOG_WRITE_SIZE*write_length);
		if (n == -1){
			if (errno == EINTR){
				if (!isatty(file) && (lseek(file, now_pos, SEEK_SET) == -1))
					goto error;
				else
					continue;
			}else
				goto error;
		}
		total += n;

		if (total == length)
			break;
	}
	return 0;
error:
	return -1;
}

__fly_static int __fly_log_write(fly_logfile_t file, fly_logcont_t *lc)
{
	char phd[FLY_LOG_PLACE_SIZE];
	int phd_len;

	phd_len = __fly_placeholder(phd, FLY_LOG_PLACE_SIZE, lc->when);
	if (phd_len < 0 || phd_len >= FLY_LOG_PLACE_SIZE)
		return -1;

#define FLY_LOG_WRITE_SUCCESS			1
#define FLY_LOG_WRITE_WAIT				0
#define FLY_LOG_WRITE_ERROR				-1
	if (!(lc->__log->flag&__FLY_LOGFILE_INIT_STDOUT) && \
			!(lc->__log->flag&__FLY_LOGFILE_INIT_STDERR)){
		switch (__fly_log_lock(file, &lc->lock)){
		case FLY_LOG_LOCK_SUCCESS:
			break;
		case FLY_LOG_LOCK_WAIT:
			return FLY_LOG_WRITE_WAIT;
		case FLY_LOG_LOCK_ERROR:
			return FLY_LOG_WRITE_ERROR;
		}
	}

	/* getting file lock */
	/* write to log place holder */
	if (__fly_write(file, phd_len, phd) == -1)
		goto error;
	/* write to log body */
	if (__fly_write(file, lc->contlen, lc->content) == -1)
		goto error;

	/* release file lock */
	goto success;
error:
	if (!(lc->__log->flag&__FLY_LOGFILE_INIT_STDOUT) && \
			!(lc->__log->flag&__FLY_LOGFILE_INIT_STDERR))
		__fly_log_unlock(file, &lc->lock);
	return -1;
success:
	if (!(lc->__log->flag&__FLY_LOGFILE_INIT_STDOUT) && \
			!(lc->__log->flag&__FLY_LOGFILE_INIT_STDERR))
		__fly_log_unlock(file, &lc->lock);
	return 0;
}

__fly_static __fly_log_t *__fly_log_from_type(fly_log_t *lt, fly_log_e type)
{
	switch(type){
	case FLY_LOG_ACCESS:
		return lt->access;
	case FLY_LOG_ERROR:
		return lt->error;
	case FLY_LOG_NOTICE:
		return lt->notice;
	default:
		return NULL;
	}
}

__fly_static int __fly_log_write_logcont(fly_logcont_t *lc)
{
	if (lc->log != NULL && (lc->__log->flag & __FLY_LOGFILE_INIT_STDOUT))
		if (__fly_log_write(STDOUT_FILENO, lc) == -1)
			return -1;
	if (lc->log != NULL && (lc->__log->flag & __FLY_LOGFILE_INIT_STDERR))
		if (__fly_log_write(STDERR_FILENO, lc) == -1)
			return -1;

	return __fly_log_write(lc->__log->file, lc);
}

int fly_logcont_setting(fly_logcont_t *lc, size_t content_length)
{
	if (lc == NULL)
		return -1;

	lc->contlen = content_length;
	lc->content = fly_pballoc(lc->log->pool, content_length);
	if (lc->content == NULL)
		return -1;
	memset(lc->content, '\0', content_length);

	return 0;
}

fly_logcont_t *fly_logcont_init(fly_log_t *log, fly_log_e type)
{
	if (!log || !log->pool)
		return NULL;

	fly_logcont_t *cont;
	cont = fly_pballoc(log->pool, sizeof(fly_logcont_t));
	if (cont == NULL)
		return NULL;

	cont->log = log;
	cont->__log = __fly_log_from_type(log, type);
	cont->type = type;
	cont->content = NULL;
	cont->contlen = 0;
	fly_time_null(cont->when);
	return cont;
}

void __fly_logcont_release(fly_logcont_t *logcont)
{
	if (logcont == NULL)
		return;

	fly_pbfree(logcont->log->pool, logcont->content);
	fly_pbfree(logcont->log->pool, logcont);
}

int fly_log_event_handler(fly_event_t *e)
{
	__unused fly_logcont_t *content;

	content = (fly_logcont_t *) e->event_data;
	__fly_log_write_logcont(content);
	__fly_logcont_release(content);
	return 0;
}

int fly_log_now(fly_time_t *t)
{
	return gettimeofday(t, NULL);
}

__attribute__ ((format (printf, 2, 3)))
void fly_notice_direct_log(fly_log_t *log, const char *fmt, ...)
{
	va_list va;
	fly_logcont_t *lc;

	lc = fly_logcont_init(log, FLY_LOG_NOTICE);
	if (lc == NULL)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_ELOG,
			"can't ready for log content."
		);
	if (fly_logcont_setting(lc, FLY_NOTICE_DIRECT_LOG_MAXLENGTH) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_ELOG,
			"can't ready for setting of log content."
		);

	va_start(va, fmt);
	vsnprintf(lc->content, lc->contlen, fmt, va);
	va_end(va);

	lc->contlen = strlen(lc->content);
	if (fly_log_now(&lc->when) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_ELOG,
			"can't set log time."
		);

	if (__fly_log_write_logcont(lc) == -1)
		FLY_EMERGENCY_ERROR(
			FLY_EMERGENCY_STATUS_ELOG,
			"log write error."
		);
	__fly_logcont_release(lc);
}

int fly_log_event_register(fly_event_manager_t *manager, struct fly_logcont *lc)
{
	fly_event_t *e;

	e = fly_event_init(manager);
	if (fly_unlikely_null(e))
		return -1;

	fly_log_now(&lc->when);
	e->read_or_write = FLY_WRITE;
	e->fd = lc->__log->file;
	FLY_EVENT_HANDLER(e, fly_log_event_handler);
	e->flag = 0;
	e->tflag = 0;
	e->eflag = 0;
	e->expired = false;
	e->available = false;
	e->event_data = (void *) lc;
	fly_event_regular(e);
	fly_time_zero(e->timeout);

	return fly_event_register(e);
}

const char *fly_log_path(void)
{
	return fly_config_value_str(FLY_LOG_PATH);
}

bool fly_log_stdout(void)
{
	return fly_config_value_bool(FLY_LOG_STDOUT);
}

bool fly_log_stderr(void)
{
	return fly_config_value_bool(FLY_LOG_STDERR);
}
