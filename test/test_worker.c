#include <assert.h>
#include <stdlib.h>
#include "worker.h"
#include "server.h"
#include "response.h"

#define TEST_PORT			"1234"

fly_response_t *hello(fly_request_t *request)
{
	fly_response_t *res;
	res = fly_response_init();
	if (res == NULL)
		return NULL;
	res->header = fly_header_init();
	res->body = fly_body_init();
	if (fly_header_add(res->header, fly_header_name_length("Hello"), fly_header_value_length("World")) == -1)
		return NULL;
	if (fly_header_add(res->header, fly_header_name_length("Connection"), fly_header_value_length("keep-alive")) == -1)
		return NULL;
	if (fly_body_setting(res->body, "Hello Body", strlen("Hello Body")) == -1)
		return NULL;
	res->status_code = _200;
	res->version = V1_1;
	return res;
}

int main()
{
	fly_context_t *ctx;
	fly_route_reg_t *reg;

	if (setenv(FLY_PORT_ENV, TEST_PORT, 1) == -1)
		return -1;

	ctx = fly_context_init();

	if (fly_mount_init(ctx) == -1)
		return -1;
	if (fly_mount(ctx, "./mnt") == -1)
		return -1;

	reg = ctx->route_reg;
//	if (fly_register_route(reg, hello, "/", GET, 0) == -1)
//			return -1;
	fly_worker_process(ctx, NULL);
}
