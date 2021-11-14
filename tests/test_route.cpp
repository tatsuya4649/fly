
#ifdef __cplusplus
extern "C"{
	#include "route.h"
}
#endif
#include <gtest/gtest.h>
#include <iostream>

TEST(APITEST, fly_route_reg_init)
{
	EXPECT_TRUE(fly_route_reg_init() == NULL);
}

TEST(APITEST, fly_route_init)
{
	EXPECT_TRUE(fly_route_init() != -1);
	EXPECT_TRUE(fly_route_release() != -1);
}

fly_response_t *test_func(fly_request_t *request)
{
	return NULL;
}

TEST(APITEST, fly_register_route)
{
	fly_route_reg_t *reg;
	fly_pool_t *old;

	EXPECT_TRUE(fly_route_init() != -1);
	EXPECT_TRUE(fly_route_pool != NULL);
	EXPECT_TRUE((reg = fly_route_reg_init()) != NULL);
	/* Success register */
	EXPECT_TRUE(fly_register_route(reg, test_func, "/", GET, 0) != -1);
	/* Failure register (route_pool is NULL) */
	old = fly_route_pool;
	fly_route_pool = NULL;
	EXPECT_TRUE(fly_route_release() == -1);
	fly_route_pool = old;
	EXPECT_TRUE(fly_route_release() != -1);
}

TEST(APITEST, fly_found_route)
{
	fly_route_reg_t *reg;
	fly_route_t *route;

	EXPECT_TRUE(fly_route_init() != -1);
	EXPECT_TRUE(fly_route_pool != NULL);
	EXPECT_TRUE((reg = fly_route_reg_init()) != NULL);
	EXPECT_TRUE(fly_register_route(reg, test_func, "/", GET, 0) != -1);

	route = fly_found_route(reg, "/", GET);
	EXPECT_TRUE(route != NULL);

	/* not found uri */
	route = fly_found_route(reg, "/user", GET);
	EXPECT_TRUE(route == NULL);

	/* reg NULL */
	route = fly_found_route(NULL, "/", GET);
	EXPECT_TRUE(route == NULL);
	EXPECT_TRUE(fly_route_release() != -1);
}
