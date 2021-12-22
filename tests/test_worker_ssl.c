#include <assert.h>
#include <stdlib.h>
#include "worker.h"
#include "server.h"
#include "response.h"
#include "ssl.h"
#include "conf.h"

#define TEST_PORT			"1234"

int main()
{
	fly_worker_t *worker;
	fly_context_t *ctx;
	struct fly_pool_manager pm;

	if (setenv(FLY_CONFIG_PATH, "tests/https_test.conf", 1) == -1)
		return -1;

	/* load config file */
	fly_parse_config_file(NULL);

	fly_bllist_init(&pm.pools);
	ctx = fly_context_init(&pm, NULL);

	if (fly_mount_init(ctx) == -1){
		perror("mount_init");
		return -1;
	}
	if (fly_mount(ctx, "./tests/mnt") == -1){
		perror("mount");
		return -1;
	}

	worker = fly_worker_init(ctx);
	fly_worker_process(ctx, worker);
}
