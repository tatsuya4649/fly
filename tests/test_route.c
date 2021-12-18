#include <stdio.h>
#include "route.h"
#include "context.h"
#include "alloc.h"

fly_response_t *test_handler(fly_request_t *request __fly_unused, fly_route_t *route __fly_unused, void *data __fly_unused)
{
	return NULL;
}

int main()
{
	fly_context_t ctx;
	struct fly_pool_manager *pm;
	fly_pool_t *pool;
	fly_route_reg_t *rr;

	pm = fly_pool_manager_init();
	assert(pm != NULL);


	pool = fly_create_pool(pm, 1);
	ctx.pool = pool;

	rr = fly_route_reg_init(&ctx);
	assert(rr != NULL);

#define TEST_REGISTER_ROUTE(__uri)					\
			(fly_register_route(rr, test_handler, __uri, strlen(__uri), GET, 0, NULL, NULL))

	/* Register Route Test (INT) */
#define TEST1_URI_INT1					"/test1/{test: int}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_INT1) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_INT1);
#define TEST1_URI_INT2					"/test1/{test:int}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_INT2) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_INT2);
#define TEST1_URI_INT3					"/test1/{test:int }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_INT3) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_INT3);
#define TEST1_URI_INT4					"/test1/{test: int }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_INT4) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_INT4);
#define TEST1_URI_INT5					"/test1/{test:		int		 }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_INT5) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_INT5);
	/* Register Route Test (FLOAT) */
#define TEST1_URI_FLOAT1				"/test1/{test: float}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_FLOAT1) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_FLOAT1);
#define TEST1_URI_FLOAT2				"/test1/{test:float}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_FLOAT2) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_FLOAT2);
#define TEST1_URI_FLOAT3				"/test1/{test:float }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_FLOAT3) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_FLOAT3);
#define TEST1_URI_FLOAT4				"/test1/{test: float }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_FLOAT4) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_FLOAT4);
#define TEST1_URI_FLOAT5				"/test1/{test:		 float			}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_FLOAT5) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_FLOAT5);
	/* Register Route Test (BOOL) */
#define TEST1_URI_BOOL1					"/test1/{test: bool}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BOOL1) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_BOOL1);
#define TEST1_URI_BOOL2					"/test1/{test:bool}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BOOL2) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_BOOL2);
#define TEST1_URI_BOOL3					"/test1/{test:bool }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BOOL3) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_BOOL3);
#define TEST1_URI_BOOL4					"/test1/{test: bool }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BOOL4) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_BOOL4);
#define TEST1_URI_BOOL5					"/test1/{test:		 bool			}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BOOL5) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_BOOL5);
	/* Register Route Test (STR) */
#define TEST1_URI_STR1					"/test1/{test: str}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_STR1) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_STR1);
#define TEST1_URI_STR2					"/test1/{test:str}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_STR2) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_STR2);
#define TEST1_URI_STR3					"/test1/{test:str }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_STR3) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_STR3);
#define TEST1_URI_STR4					"/test1/{test: str }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_STR4) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_STR4);
#define TEST1_URI_STR5					"/test1/{test:	 	str		 }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_STR5) == FLY_REGISTER_ROUTE_SUCCESS);
	printf("=========== Passed %s\n", TEST1_URI_STR5);
	/* Register Route Test (UNKNOWN) */
#define TEST1_URI_UNKNOWN1					"/test1/{test: INT}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_UNKNOWN1) == FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR);
	printf("=========== Passed %s\n", TEST1_URI_UNKNOWN1);
#define TEST1_URI_UNKNOWN2					"/test1/{test: inT}"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_UNKNOWN2) == FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR);
	printf("=========== Passed %s\n", TEST1_URI_UNKNOWN2);
	/* Register Route Test (Syntax Error) */
#define TEST1_URI_SYNT1					"/test1/{{test: int }"
	assert(TEST_REGISTER_ROUTE(TEST1_URI_SYNT1) == FLY_REGISTER_ROUTE_PATH_PARAM_SYNTAX_ERROR);
	printf("=========== Passed %s\n", TEST1_URI_SYNT1);
	/* Register Route Test (Bracket Error) */
#define TEST1_URI_BRACKET1					"/test1/{test: int "
	assert(TEST_REGISTER_ROUTE(TEST1_URI_BRACKET1) == FLY_REGISTER_ROUTE_PATH_PARAM_NO_RIGHT_BRACKET);
	printf("=========== Passed %s\n", TEST1_URI_BRACKET1);
}
