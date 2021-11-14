
#ifdef __cplusplus
extern "C"{
	#include "math.h"
}
#endif
#include <gtest/gtest.h>

TEST(MATH, fly_number_digits)
{
	EXPECT_TRUE(fly_number_digits(0) == 0);
	EXPECT_TRUE(fly_number_digits(1) == 1);
	EXPECT_TRUE(fly_number_digits(10) == 2);
	EXPECT_TRUE(fly_number_digits(100) == 3);
}
