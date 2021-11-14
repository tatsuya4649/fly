
#ifdef __cplusplus
extern "C"{
	#include "util.h"
}
#endif
#include <string.h>
#include <gtest/gtest.h>

TEST(URIL, fly_util_strcpy)
{
	char src[] = "Hello world";
	char dist[100];
	EXPECT_TRUE(fly_until_strcpy(dist, src, NULL, NULL) == 1);

	for (int i=0; i<strlen(src); i++)
		EXPECT_TRUE(src[i] ==  dist[i]);

	EXPECT_TRUE(fly_until_strcpy(dist, src, NULL, src+strlen(src)-10) == -1);
	EXPECT_TRUE(fly_until_strcpy(dist, src, "H", NULL) == 0);
}
