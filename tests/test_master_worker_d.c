#include <assert.h>
#include <stdlib.h>
#include "master.h"
#include "worker.h"

int main()
{
	fly_master_t *master;

	if (setenv(FLY_CONFIG_PATH, "tests/test.conf", 1) == -1)
		return 1;

	fly_parse_config_file();

	printf("Hweer?\n");
	assert((master=fly_master_init(NULL)) != NULL);
	printf("Hweer?\n");

	assert(fly_mount_init(master->context) != -1);
	printf("Hweer?\n");
	assert(fly_mount(master->context, "./tests/mnt") != -1);
	printf("Hweer?\n");
	assert(fly_mount(master->context, "./tests/mnt2") != -1);
	printf("Hweer?\n");

	assert(fly_daemon(master->context) == 0);
	fly_master_worker_spawn(master, fly_worker_process);
	fly_master_process(master);
}
