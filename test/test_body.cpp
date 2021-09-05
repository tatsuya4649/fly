#ifdef __cplusplus
extern "C"{
	#include "body.h"
}
#endif
#include <gtest/gtest.h>

TEST(BODY, fly_body_init)
{
	EXPECT_TRUE(fly_body_init() != NULL);
}

TEST(BODY, fly_body_release)
{
	fly_body_t *body;
	EXPECT_TRUE((body=fly_body_init()) != NULL);
	EXPECT_TRUE(fly_body_release(body) != -1);
}

TEST(BODY, fly_body_setting)
{
	fly_body_t *body;
	fly_bodyc_t buffer[] = "body";
	EXPECT_TRUE((body=fly_body_init()) != NULL);
	EXPECT_TRUE(fly_body_setting(body, buffer) == 0);
	EXPECT_TRUE(fly_body_setting(NULL, buffer) == -1);
	EXPECT_TRUE(fly_body_setting(body, NULL) == -1);
	EXPECT_TRUE(fly_body_setting(NULL, NULL) == -1);
}

TEST(BODY, fly_get_body_ptr)
{
	char buffer[] = "\r\n\r\n";
	EXPECT_TRUE(fly_get_body_ptr(buffer) != NULL);
	EXPECT_TRUE(fly_get_body_ptr(NULL) == NULL);
}
