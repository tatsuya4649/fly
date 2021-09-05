
#ifdef __cplusplus
extern "C"{
#include "log.h"
#include "util.h"
int __fly_log_write(fly_log_fp *lfp, char *logbody);
}
#endif
#include <gtest/gtest.h>

TEST(LOGTEST, fly_log_init)
{
	fly_log_t *lt;
	lt = fly_log_init();
	EXPECT_TRUE(lt != NULL);
	EXPECT_TRUE(fly_log_release(lt) != -1);
}

TEST(LOGTEST, __fly_log_write)
{
	fly_log_t *lt;
	lt = fly_log_init();
	EXPECT_TRUE(lt != NULL);
	EXPECT_TRUE(__fly_log_write(lt->access->fp, (char *) "test") == 0);
	EXPECT_TRUE(fly_log_release(lt) != -1);
}
