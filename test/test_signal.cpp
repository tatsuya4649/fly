
#ifdef __cplusplus
extern "C"{
	#include "fsignal.h"
}
#endif
#include <gtest/gtest.h>

TEST(SignalTest, fly_signal_init)
{
	EXPECT_EQ(0, fly_signal_init());
}
