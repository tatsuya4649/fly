#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "master.h"

#define WORKERS					"5"
#define PORT					"1234"
static void worker(__unused fly_context_t *ctx, void *workers)
{
	int rtime;
	fly_worker_i *info;
	info = (fly_worker_i *) workers;
	printf("Start Worker %d[%d]: %d\n", info->id, getpid(), (int) info->start);
	srand(time(NULL));

	rtime = rand() % 100;
	sleep(rtime);
}

int main()
{
	fly_context_t *ctx;
	pid_t pid;

	/* config setting */
	fly_parse_config_file();

	/* master signal test */
	assert((ctx=fly_master_init()) != NULL);

	/* create pid */
	assert(fly_create_pidfile() != -1);

	/* mount setting */
	assert(fly_mount_init(ctx) != -1);
	assert(fly_mount(ctx, "./mnt") != -1);

	/* master fork process */
	fly_master_worker_spawn(ctx, worker);

	fly_master_process(ctx);
}
