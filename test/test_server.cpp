
#ifdef __cplusplus
extern "C"{
#include <server.h>
}
#endif
#include <gtest/gtest.h>

TEST(ServerTest, fly_socket_init_test){
	int sockfd;
	sockfd = fly_socket_init();
	EXPECT_NE(-1, sockfd);
	EXPECT_TRUE(fly_socket_release(sockfd) != -1);
}
