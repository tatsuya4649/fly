#ifndef _WORKER_H
#define _WORKER_H

#include <unistd.h>
#include <time.h>

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

void fly_worker_process(void);

#endif
