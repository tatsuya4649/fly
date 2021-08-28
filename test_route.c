#include "route.h"
#include <stdio.h>

fly_response_t *test_function(__unused fly_request_t *request)
{
	printf("Hello World\n");
	return NULL;
}
