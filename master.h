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
#include "fsignal.h"
#include "config.h"

struct fly_master{
	int req_workers;
	int now_workers;
};
typedef struct fly_master fly_master_t;
extern fly_master_t fly_master_info;

extern fly_signal_t fly_master_signals[];

int fly_master_daemon(void);
int fly_master_signal(void);
int fly_create_pidfile(void);
int fly_remove_pidfile(void);
int fly_master_init(void);
int fly_master_waiting_for_signal(void);
int fly_master_spawn(void (*proc)(void *), void *data);

#define FLY_ROOT_DIR		("/")
#define __FLY_DEVNULL		("/dev/null")
#define FLY_DAEMON_STDOUT	__FLY_DEVNULL
#define FLY_DAEMON_STDERR	__FLY_DEVNULL
#define FLY_DAEMON_STDIN	__FLY_DEVNULL
#define fly_master_pid		getpid
#define FLY_WORKERS_ENV		"FLY_WORKERS"

#define FLY_MASTER_SIG_COUNT				(sizeof(fly_master_signals)/sizeof(fly_signal_t))

#endif
