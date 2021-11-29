#include "util.h"
#include <sys/resource.h>
#include "context.h"

int fly_until_strcpy(char *dist, char *src, const char *target, char *limit_addr)
{
	while(*src != '\0'){
		if (limit_addr != NULL && src >= limit_addr)
			return -1;

		if (target != NULL){
			for (size_t i=0;i<strlen(target); i++){
				if (target[i] == *src){
					dist[i] = '\0';
					return 0;
				}
			}
		}

		*dist++ = *src++;
	}
	return 1;
}

int fly_daemon(fly_context_t *ctx)
{
	struct rlimit fd_limit;
	int nullfd;

	ctx->daemon = true;
	switch(fork()){
	case -1:
		return FLY_DAEMON_FORK_ERROR;
	case 0:
		break;
	default:
		exit(0);
	}

	/* child process only */
	if (setsid() == -1)
		return FLY_DAEMON_SETSID_ERROR;

	/* for can't access tty */
	switch(fork()){
	case -1:
		return FLY_DAEMON_FORK_ERROR;
	case 0:
		break;
	default:
		exit(0);
	}
	/* grandchild process only */
	umask(0);
	if (chdir(FLY_ROOT_DIR) == -1)
		return FLY_DAEMON_CHDIR_ERROR;

	if (getrlimit(RLIMIT_NOFILE, &fd_limit) == -1)
		return FLY_DAEMON_GETRLIMIT_ERROR;

	for (int i=0; i<(int) fd_limit.rlim_cur; i++){
		if (is_fly_log_fd(i, ctx))
			continue;
		if (is_fly_listen_socket(i, ctx))
			continue;

		if (close(i) == -1 && errno != EBADF)
			return FLY_DAEMON_CLOSE_ERROR;
	}

	nullfd = open(FLY_DEVNULL, O_RDWR);
	if (nullfd == -1 || nullfd != STDIN_FILENO)
		return FLY_DAEMON_OPEN_ERROR;

	if (dup2(nullfd, STDOUT_FILENO) == -1)
		return FLY_DAEMON_DUP_ERROR;
	if (dup2(nullfd, STDERR_FILENO) == -1)
		return FLY_DAEMON_DUP_ERROR;

	return FLY_DAEMON_SUCCESS;
}


ssize_t fly_sendfile(int out_fd, int in_fd, off_t *offset, size_t count)
{
#ifdef HAVE_SYS_SENDFILE_H
	return sendfile(out_fd, in_fd, offset, count);
#else
#define FLY_SENDFILE_BUFFER			4096
	size_t __total = 0;
	char __buf[FLY_SENDFILE_BUFFER];

	if (offset){
		if (lseek(in_fd, *offset, SEEK_SET) == -1)
			return -1;
	}
	while(__total<count){
		ssize_t readsize;
		ssize_t writesize=0;

		readsize = read(in_fd, __buf, FLY_SENDFILE_BUFFER);
		if (readsize == -1)
			return -1;

		while(writesize < readsize){
			ssize_t res;
			res = write(out_fd, __buf, (size_t) readsize-writesize);
			if (res == -1)
				return -1;

			writesize += res;
		}
		__total += readsize;
	}

	*offset += count;
	return count;
#endif
}

