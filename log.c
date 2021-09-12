#include "log.h"
#include <errno.h>

__fly_static __fly_log_t *__fly_log_from_type(fly_log_t *lt, fly_log_e type);
__fly_static int __fly_log_write_logcont(fly_logcont_t *lc);
__fly_static int __fly_log_write(fly_logfile_t file, fly_logcont_t *lc);
__fly_static int __fly_placeholder(char *plh, size_t plh_size, fly_time_t t);

fly_pool_t *fly_log_pool = NULL;
__fly_static int __fly_make_logdir(fly_path_t *dir, size_t dirsize);

__fly_static fly_path_t *__fly_log_path(const char *env)
{
	return getenv(env);
}

__fly_static fly_path_t *__fly_error_log_path(void)
{
	char *path;
	path = __fly_log_path(FLY_ERRORLOG_ENV);
	return path != NULL ? (fly_path_t *) path : FLY_ERRORLOG_DEFAULT;
}

__fly_static fly_path_t *__fly_access_log_path(void)
{
	char *path;
	path = __fly_log_path(FLY_ACCESLOG_ENV);
	return path != NULL ? (fly_path_t *) path : FLY_ACCESLOG_DEFAULT;
}

__fly_static fly_path_t *__fly_notice_log_path(void)
{
	char *path;
	path = __fly_log_path(FLY_NOTICLOG_ENV);
	return path != NULL ? (fly_path_t *) path : FLY_NOTICLOG_DEFAULT;
}

__fly_static __fly_log_t *__fly_logfile_init(const fly_path_t *fly_log_path)
{
	__fly_log_t *lt;
	fly_logfile_t lfile;

	lt = fly_pballoc(fly_log_pool, sizeof(__fly_log_t));
	if (lt == NULL)
		return NULL;

	if (__fly_make_logdir((fly_path_t *) fly_log_path, strlen(fly_log_path)) < 0)
		return NULL;

	lfile = open(fly_log_path, O_RDWR|O_CREAT, FLY_LOGFILE_MODE);
	if (lfile == -1)
		return NULL;

	lt->file = lfile;
	strcpy(lt->log_path, fly_log_path);
	return lt;
}

#define __fly_log_path(name)			__fly_ ## name ## _log_path()

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

			if (fly_fs_isdir(path) <= 0)
				return FLY_ENFOUNDDIR;

			only_word = false;
		}

		if (*ptr == '\0'){
			if (only_word && fly_fs_isdir(dir) <= 0)
				return FLY_ENFOUNDDIR;
			return FLY_SUCCESS;
		}

		ptr++;
		n++;
	}
	return FLY_EBUFLEN;
}

fly_log_t *fly_log_init(void)
{
	fly_log_t *lt;
	__fly_log_t *alp, *elp, *nlp;

	if (fly_log_pool == NULL)
		fly_log_pool = fly_create_pool(FLY_LOG_POOL_SIZE);

	if (!fly_log_pool)
		goto error;

	lt = fly_pballoc(fly_log_pool, sizeof(fly_log_t));
	if (!lt)
		goto error;

	alp = __fly_logfile_init(__fly_log_path(access));
	elp = __fly_logfile_init(__fly_log_path(error));
	nlp = __fly_logfile_init(__fly_log_path(notice));
	if (!alp || !elp || !nlp)
		goto error;

	lt->access = alp;
	lt->error = elp;
	lt->notice = nlp;
	lt->pool = fly_log_pool;
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

	return fly_delete_pool(&lt->pool);
}

#include "ftime.h"
__fly_static int __fly_placeholder(char *plh, size_t plh_size, fly_time_t t)
{
	char ftime[FLY_TIME_MAX];

	if (fly_logtime(ftime, FLY_TIME_MAX, &t) == -1)
		return -1;
	return snprintf(plh, plh_size, "%s [%d]: ", ftime, getpid());
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
	if (lseek(file, 0, SEEK_END) == -1)
		return -1;
	while(true){
#ifndef FLY_LOG_WRITE_SIZE
#define FLY_LOG_WRITE_SIZE		sizeof(fly_logc_t)
#endif
		int now_pos, n, write_length;
		write_length = length - total;
		if ((now_pos = lseek(file, 0, SEEK_CUR)) == -1)
				goto error;

		n = write(file, content, FLY_LOG_WRITE_SIZE*write_length);
		if (n == -1){
			if (errno == EINTR){
				if (lseek(file, now_pos, SEEK_SET) == -1)
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
	switch (__fly_log_lock(file, &lc->lock)){
	case FLY_LOG_LOCK_SUCCESS:
		break;
	case FLY_LOG_LOCK_WAIT:
		return FLY_LOG_WRITE_WAIT;
	case FLY_LOG_LOCK_ERROR:
		return FLY_LOG_WRITE_ERROR;
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
	__fly_log_unlock(file, &lc->lock);
	return -1;
success:
	__fly_log_unlock(file, &lc->lock);
	return 0;
}

__fly_static __fly_log_t *__fly_log_from_type(fly_log_t *lt, fly_log_e type)
{
	switch(type){
	case ACCESS:
		return lt->access;
	case ERROR:
		return lt->error;
	case NOTICE:
		return lt->notice;
	default:
		return NULL;
	}
}

__fly_static int __fly_log_write_logcont(fly_logcont_t *lc)
{
	__fly_log_t *_lt;
	_lt = __fly_log_from_type(lc->log, lc->type);
	if (_lt == NULL)
		return -1;

	return __fly_log_write(_lt->file, lc);
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
	cont->type = type;
	return cont;
}

int fly_logcont_release(fly_logcont_t *logcont)
{
	if (logcont == NULL)
		return -1;

	return 0;
}

int fly_log_event_handler(fly_event_t *e)
{
	__unused fly_logcont_t *content;

	content = (fly_logcont_t *) e->event_data;
	__fly_log_write_logcont(content);
	fly_logcont_release(content);
	return fly_event_unregister(e);
}

int fly_log_now(fly_time_t *t)
{
	return gettimeofday(t, NULL);
}
