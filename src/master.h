#ifndef _MASTER_H
#define _MASTER_H

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/resource.h>
#include <sys/inotify.h>
#include "fly.h"
#include "fsignal.h"
#include "conf.h"
#include "alloc.h"
#include "worker.h"
#include "context.h"

enum fly_proc_type{
	WORKER,
};

typedef enum fly_proc_type fly_proc_type;
struct fly_master{
	pid_t						pid;
	int							req_workers;
	int 						now_workers;
	void		(*worker_process)(fly_context_t *ctx, void *data);
	struct fly_pool_manager		*pool_manager;
	struct fly_event_manager	*event_manager;

	struct fly_bllist			workers;
	fly_context_t				*context;

	struct fly_bllist			reload_filepath;
	size_t						reload_pathcount;
	fly_bit_t					detect_reload:1;
};

struct fly_watch_path{
	int							wd;
	const char					*path;
	struct fly_bllist			blelem;
	fly_bit_t					configure:1;
};
#define FLY_MASTER_POOL_SIZE				100
typedef struct fly_master fly_master_t;

extern fly_signal_t fly_master_signals[];

int fly_master_daemon(fly_context_t *ctx);
int fly_create_pidfile(void);
int fly_create_pidfile_noexit(void);
void fly_remove_pidfile(void);
//fly_context_t *fly_master_init(void);
fly_master_t *fly_master_init(void);
void fly_master_release(fly_master_t *master);
fly_context_t *fly_master_release_except_context(fly_master_t *master);
/*
 * waiting for signal foever. wait or end.
 */

#define FLY_MASTER_CONTINUE				0
#define FLY_MASTER_SIGNAL_END			1
#define FLY_MASTER_RELOAD				2
__direct_log int fly_master_process(fly_master_t *master);
void fly_master_worker_spawn(fly_master_t *master, void (*proc)(fly_context_t *, void *));

#define __FLY_DEVNULL		("/dev/null")
#define FLY_DAEMON_STDOUT	__FLY_DEVNULL
#define FLY_DAEMON_STDERR	__FLY_DEVNULL
#define FLY_DAEMON_STDIN	__FLY_DEVNULL
#define fly_master_pid		getpid
#define FLY_WORKER			"FLY_WORKER"
#define FLY_WORKER_MAX		"FLY_WORKER_MAX"
#define FLY_MASTER_SIG_COUNT				(sizeof(fly_master_signals)/sizeof(fly_signal_t))

#define FLY_CREATE_PIDFILE					"FLY_CREATE_PIDFILE"
bool fly_is_create_pidfile(void);
void fly_master_setreload(fly_master_t *master, const char *reload_filepath, bool configure);

#endif
