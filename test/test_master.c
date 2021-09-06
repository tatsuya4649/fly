#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "master.h"

#define WORKERS					"5"
static void worker(void *workers)
{
	int rtime;
	fly_worker_i *info;
	info = (fly_worker_i *) workers;
	printf("Start Worker %d[%d]: %d\n", info->id, getpid(), (int) info->start);
	srand(time(NULL));

	rtime = rand() % 10;
	sleep(rtime);
}

int main()
{
	pid_t pid;

	assert(setenv(FLY_WORKERS_ENV, WORKERS, 1) != -1);
	/* master signal test */
	assert(fly_master_init() == 0);

	/* create pid */
	assert(fly_create_pidfile() != -1);

	/* master fork process */
	assert(fly_master_worker_spawn(worker) != -1);

	assert(fly_master_waiting_for_signal() == 0);
	/* master daemon test */
//	pid = getpid();
//	assert(fly_master_daemon() == 0);
//	assert(!isatty(STDIN_FILENO));
//	assert(!isatty(STDOUT_FILENO));
//	assert(!isatty(STDERR_FILENO));
//
//	assert(pid != getpid());
}
