#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include "master.h"

#define WORKERS					"10"
static void worker(void *workers)
{
	printf("Start Worker %d\n", *((int *) workers));
	sleep(3);
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
	assert(fly_master_spawn(worker, NULL) != -1);

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
