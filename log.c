#include "log.h"
#include <errno.h>

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
	fly_log_fp *lfp;

	lt = fly_pballoc(fly_log_pool, sizeof(__fly_log_t));
	if (lt == NULL)
		return NULL;

	if (__fly_make_logdir((fly_path_t *) fly_log_path, strlen(fly_log_path)) < 0)
		return NULL;

	lfp = fopen(fly_log_path, "a");
	if (lfp == NULL)
		return NULL;

	lt->fp = lfp;
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
	lt->pool = &fly_log_pool;
	return lt;
error:
	return NULL;
}
int fly_log_release(fly_log_t *lt)
{
	if (!lt || !lt->pool)
			return -1;
	if (fclose(lt->access->fp) == EOF)
		return -1;
	if (fclose(lt->error->fp) == EOF)
		return -1;
	if (fclose(lt->notice->fp) == EOF)
		return -1;

	return fly_delete_pool(lt->pool);
}

__fly_static int __fly_placeholder(char *plh, size_t plh_size)
{
	char ftime[FLY_TIME_MAX];

	if (fly_logtime(ftime, FLY_TIME_MAX) == -1)
		return -1;
	return snprintf(plh, plh_size, "%s [%d]", ftime, getpid());
}

__fly_static int __fly_log_write(fly_log_fp *lfp, char *logbody)
{
	char plh[FLY_LOG_PLACE_SIZE], body[FLY_LOG_BODY_SIZE];
	int res;

	res = __fly_placeholder(plh, FLY_LOG_PLACE_SIZE);
	if (res < 0 || res >= FLY_LOG_PLACE_SIZE)
		goto error;

	res = snprintf(body, FLY_LOG_BODY_SIZE, "%s: %s\n", plh, logbody);
	if (res < 0 || res >= FLY_LOG_BODY_SIZE)
		goto error;

	while(true){
#ifndef FLY_LOG_WRITE_SIZE
#define FLY_LOG_WRITE_SIZE		1
#endif
		int now_pos, n;
		if ((now_pos = fseek(lfp, 0, SEEK_CUR)) != 0)
				goto error;

		if ((n=fwrite(body, res, FLY_LOG_WRITE_SIZE, lfp)) == FLY_LOG_WRITE_SIZE)
			break;

		if (n < 0){
			if (errno == EINTR){
				if (fseek(lfp, 0, SEEK_CUR) != 0)
					goto error;
				else
					continue;
			}else
				goto error;
		}
	}

	return 0;
error:
	return -1;
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

int fly_log_write(fly_log_t *lt, fly_log_e type, char *logbody)
{
	__fly_log_t *l;
	l = __fly_log_from_type(lt, type);
	if (!l)
		return -1;

	return __fly_log_write(l->fp, logbody);
}

int fly_error_log_write(fly_log_t *lt, char *logbody)
{
	return fly_log_write(lt, ERROR, logbody);
}
int fly_access_log_write(fly_log_t *lt, char *logbody)
{
	return fly_log_write(lt, ACCESS, logbody);
}
int fly_notice_log_write(fly_log_t *lt, char *logbody)
{
	return fly_log_write(lt, NOTICE, logbody);
}
