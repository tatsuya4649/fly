#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include "master.h"

#define WORKERS					"5"
#define PORT					"1234"
static void test_worker(__unused fly_context_t *ctx, void *)
{
	time_t rtime;

	printf("Start Worker %d: %d\n", getpid(), (int) time(NULL));
	srand(time(NULL));

	rtime = rand() % 100;
	sleep(rtime);
}

int main()
{
	fly_master_t *master;

	/* config setting */
	fly_parse_config_file();

	/* master signal test */
	assert((master=fly_master_init()) != NULL);

	/* mount setting */
	assert(fly_mount_init(master->context) != -1);
	assert(fly_mount(master->context, "./mnt") != -1);

	/* master fork process */
	fly_master_worker_spawn(master, test_worker);

	fly_master_process(master);
}
