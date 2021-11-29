#include <assert.h>
#include <stdlib.h>
#include "worker.h"
#include "server.h"
#include "response.h"
#include "ssl.h"
#include "conf.h"

#define TEST_PORT			"1234"

fly_response_t *hello(fly_request_t *request)
{
	fly_response_t *res;
	res = fly_response_init(request->ctx);
	if (res == NULL)
		return NULL;
	res->header = fly_header_init(request->ctx);
	res->body = fly_body_init(request->ctx);
	if (fly_header_add(res->header, fly_header_name_length("Hello"), fly_header_value_length("World")) == -1)
		return NULL;
	if (fly_header_add(res->header, fly_header_name_length("Connection"), fly_header_value_length("keep-alive")) == -1)
		return NULL;
	res->status_code = _200;
	res->version = V1_1;
	return res;
}

int main()
{
	fly_worker_t *worker;
	fly_context_t *ctx;
	fly_route_reg_t *reg;
	struct fly_pool_manager pm;

	if (setenv(FLY_CONFIG_PATH, "tests/https_test.conf", 1) == -1)
		return -1;

	/* load config file */
	fly_parse_config_file();

	fly_bllist_init(&pm.pools);
	ctx = fly_context_init(&pm);

	if (fly_mount_init(ctx) == -1)
		return -1;
//	if (fly_mount(ctx, "./test") == -1)
//		return -1;
//	if (fly_mount(ctx, "./lib") == -1)
//		return -1;
	if (fly_mount(ctx, "./tests/mnt") == -1)
		return -1;

	worker = fly_worker_init(ctx);
	reg = ctx->route_reg;
//	if (fly_register_route(reg, hello, "/", GET, 0) == -1)
//			return -1;
	fly_worker_process(ctx, worker);
}
