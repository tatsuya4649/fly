#include "api.h"
#include <stdio.h>

int test_function(__unused fly_request_t *request)
{
	printf("Hello World\n");
	return 0;
}
