#ifdef __cplusplus
extern "C"{
	#include "fs.h"
	#include "err.h"
	void *fly_memory_from_size(fly_pool_t *pool, fly_pool_s size);
}
#endif
#include <stdlib.h>
#include <gtest/gtest.h>

TEST(FSTEST, fly_fs_init)
{
	EXPECT_EQ(0, fly_fs_init());
}

TEST(FSTEST, fly_fs_isdir)
{
	EXPECT_EQ(1, fly_fs_isdir("."));
	EXPECT_EQ(1, fly_fs_isdir("./"));
	EXPECT_EQ(1, fly_fs_isdir("../"));
	EXPECT_EQ(-1, fly_fs_isdir("__test"));
	EXPECT_EQ(0, fly_fs_isdir("./Makefile"));
}

TEST(FSTEST, fly_fs_isfile)
{
	EXPECT_EQ(1, fly_fs_isfile("./Makefile"));
	EXPECT_EQ(1, fly_fs_isfile("Makefile"));
	EXPECT_EQ(0, fly_fs_isfile("./test"));
	EXPECT_EQ(0, fly_fs_isfile("test"));
	EXPECT_EQ(-1, fly_fs_isfile("./__test"));
}

TEST(FSTEST, fly_file_size)
{
	EXPECT_GT(fly_file_size("Makefile"), 0);
	EXPECT_EQ(fly_file_size("./test"), -1);
}

TEST(FSTEST, fly_fs_mount)
{
	/* initialize function */
	fly_fs_init();
	char *path = (char *) malloc(FLY_PATH_MAX+1);
	memset(path, 'a', FLY_PATH_MAX+1);
	path[FLY_PATH_MAX+1] = '\0';
	EXPECT_EQ(-1, fly_fs_mount(path));

	/* init_mount is NULL */
	/* Success of mount */
	init_mount = NULL;
	EXPECT_EQ(0, fly_fs_mount("./test"));
	/* Failure of mount (not found dir) */
	init_mount = NULL;
	EXPECT_EQ(-1, fly_fs_mount("./__test"));
	EXPECT_EQ(-1, fly_fs_mount(path));

	/* init_mount is not NULL */
	/* Success of mount */
	EXPECT_EQ(0, fly_fs_mount("./test"));
	EXPECT_EQ(0, fly_fs_mount("./test"));
	EXPECT_EQ(0, fly_fs_mount("."));
	/* Failure of mount (not found dir) */
	EXPECT_EQ(-1, fly_fs_mount("./__test"));
	EXPECT_EQ(-1, fly_fs_mount(path));

	fly_fs_release();
	free(path);
}

TEST(FSTEST, fly_fs_release)
{
	fly_fs_init();
	fly_fs_release();
}

TEST(FSTEST, fly_mount_number)
{
	fly_fs_init();
	EXPECT_EQ(0, fly_fs_mount("./test"));
	EXPECT_EQ(FLY_ENOTFOUND, fly_mount_number("./__test"));
	EXPECT_NE(-1, fly_mount_number("./test"));
	fly_fs_release();
}

TEST(FSTEST, fly_join_path)
{
	char path1[FLY_PATH_MAX];
	char path2[FLY_PATH_MAX];
	char result[FLY_PATH_MAX];
	fly_fs_init();

	memset(path1, 'a', FLY_PATH_MAX/2);
	path1[FLY_PATH_MAX/2] = '\0';
	memset(path2, 'b', FLY_PATH_MAX/2);
	path2[FLY_PATH_MAX/2] = '\0';

	printf("FLY_PATH_MAX: %d\n", FLY_PATH_MAX);
	printf("PATH1: %d\n", FLY_PATH_MAX/2);
	printf("PATH2: %d\n", FLY_PATH_MAX/2);
	EXPECT_EQ(-1, fly_join_path(result, path1, path2));

	char result2[FLY_PATH_MAX];
	EXPECT_EQ(-1, fly_join_path(result2, (char *) "fasdfsa/", (char *) "/dafasf"));
	char result3[FLY_PATH_MAX];
	EXPECT_EQ(0, fly_join_path(result3, (char *) ".", (char *) "test"));
	printf("%s\n", result3);

	fly_fs_release();
}

TEST(FSTEST, fly_memory_from_size)
{
	fly_fs_init();
	EXPECT_TRUE(fly_memory_from_size(fspool, XS) != NULL);
	EXPECT_TRUE(fly_memory_from_size(fspool, S) != NULL);
	EXPECT_TRUE(fly_memory_from_size(fspool, M) != NULL);
	EXPECT_TRUE(fly_memory_from_size(fspool, L) != NULL);
	EXPECT_TRUE(fly_memory_from_size(fspool, XL) != NULL);
	fly_fs_release();
}

TEST(FSTEST, fly_from_path)
{
	/* Success */
	fly_fs_init();
	int number;
	EXPECT_EQ(0, fly_fs_mount("./test"));
	number = fly_mount_number("./test");
	EXPECT_TRUE(number != -1);
	printf("Mount Number: %d\n", number);
	EXPECT_TRUE(fly_from_path(fspool, S, number, (char *) "test_fs.cpp") != NULL);
	fly_fs_release();

	/* Failure Too small size */
	fly_fs_init();
	EXPECT_EQ(0, fly_fs_mount("./test"));
	number = fly_mount_number("./test");
	EXPECT_TRUE(number != -1);
	printf("Mount Number: %d\n", number);
	EXPECT_TRUE(fly_from_path(fspool, XS, number, (char *) "test_fs.cpp") == NULL);
	fly_fs_release();

	/* Failure not exists file */
	fly_fs_init();
	EXPECT_EQ(0, fly_fs_mount("./test"));
	number = fly_mount_number("./test");
	EXPECT_TRUE(number != -1);
	printf("Mount Number: %d\n", number);
	EXPECT_TRUE(fly_from_path(fspool, XS, number, (char *) "__test_fs.cpp") == NULL);
	fly_fs_release();

}

