#include <assert.h>
#include "err.h"

#define TEST_PORT			"1234"
int main()
{
	fly_err_t *err;
	fly_context_t *ctx;
	fly_event_manager_t *manager;

	if (setenv(FLY_PORT_ENV, TEST_PORT, 1) == -1)
		return -1;
	if (setenv(FLY_STDOUT_ENV(ERROR), "Hello", 1) == -1)
		return -1;
	if (setenv(FLY_STDERR_ENV(ERROR), "Hello", 1) == -1)
		return -1;

	/* ready error system */
	if (fly_errsys_init() == -1)
		return 1;

	/* context setting */
	ctx = fly_context_init();
	if (ctx == NULL)
		return 1;
	/* event manager setting */
	manager = fly_event_manager_init(ctx);
	if (manager == NULL)
		return 1;

	/* making error */
	err = fly_err_init("Hello Error", -1, FLY_ERR_NOTICE);
	/* register error for event manager  */
	if (fly_errlog_event(manager, err) == -1)
		return 1;

	/* manager handle */
	if (fly_event_handler(manager) == -1)
		return 1;
	fly_event_manager_release(manager);
}
