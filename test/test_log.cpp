
#ifdef __cplusplus
extern "C"{
#include "log.h"
#include "util.h"
int __fly_log_write(fly_log_fp *lfp, char *logbody);
int __fly_make_logdir(fly_path_t *dir, size_t dirsize);
}
#endif
#include <string.h>
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

TEST(LOGTEST, __fly_make_logdir)
{
	const char failure[] = "/path/hello";
	EXPECT_TRUE(__fly_make_logdir((fly_path_t *) failure, strlen(failure)) < 0);

	const char failure2[] = "path";
	EXPECT_TRUE(__fly_make_logdir((fly_path_t *) failure2, strlen(failure2)) < 0);

	const char success[] = "test";
	EXPECT_TRUE(__fly_make_logdir((fly_path_t *) success, strlen(failure)) == 0);
}

TEST(LOGTEST, fly_error_log)
{
	fly_log_t *lt;
	const char sample[] = "test";

	lt = fly_log_init();
	EXPECT_TRUE(lt != NULL);
	EXPECT_TRUE(fly_error_log_write(lt, (char *) sample) == 0);

	EXPECT_TRUE(fly_log_release(lt) != -1);
}

TEST(LOGTEST, fly_access_log)
{
	fly_log_t *lt;
	const char sample[] = "test";

	lt = fly_log_init();
	EXPECT_TRUE(lt != NULL);
	EXPECT_TRUE(fly_access_log_write(lt, (char *) sample) == 0);

	EXPECT_TRUE(fly_log_release(lt) != -1);
}

TEST(LOGTEST, fly_notice_log)
{
	fly_log_t *lt;
	const char sample[] = "test";

	lt = fly_log_init();
	EXPECT_TRUE(lt != NULL);
	EXPECT_TRUE(fly_notice_log_write(lt, (char *) sample) == 0);

	EXPECT_TRUE(fly_log_release(lt) != -1);
}
