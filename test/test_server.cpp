
#ifdef __cplusplus
extern "C"{
#include <server.h>
}
#endif
#include <gtest/gtest.h>

TEST(ServerTest, fly_socket_init_test){
	EXPECT_NE(-1, fly_socket_init());
}
