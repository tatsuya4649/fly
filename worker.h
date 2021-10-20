#ifndef _WORKER_H
#define _WORKER_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <dirent.h>
#include "bllist.h"
#include "context.h"
#include "event.h"
#include "connect.h"
#include "request.h"
#include "fsignal.h"
#include "master.h"

struct fly_worker{
	pid_t						pid;
	pid_t 						ppid;
	time_t						start;

	struct fly_pool_manager		*pool_manager;
	fly_context_t				*context;

	/* use in master process */
	struct fly_master			*master;
	struct fly_bllist 			blelem;
};

typedef int fly_worker_id;

typedef struct fly_worker fly_worker_t;

__direct_log __noreturn void fly_worker_process(fly_context_t *ctx, void *data);
struct fly_worker *fly_worker_init(void);
void fly_worker_release(fly_worker_t *worker);

#define FLY_WORKER_SUCCESS_EXIT			0
void fly_worker_signal(void);
#endif
