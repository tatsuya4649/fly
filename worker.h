#ifndef _WORKER_H
#define _WORKER_H

#define _GNU_SOURCE
#include <unistd.h>
#include <time.h>
#include "context.h"
#include "event.h"

struct fly_worker{
	pid_t pid;
	pid_t ppid;
	struct fly_worker *next;
};

typedef int fly_worker_id;

struct fly_worker_info{
	fly_worker_id	id;
	time_t			start;
};
typedef struct fly_worker fly_worker_t;
typedef struct fly_worker_info fly_worker_i;

__noreturn void fly_worker_process(fly_context_t *ctx, void *data);

#endif
