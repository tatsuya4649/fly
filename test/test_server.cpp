
#ifdef __cplusplus
extern "C"{
#include "server.h"
}
#endif
#include <gtest/gtest.h>

TEST(ServerTest, fly_socket_init_test){
	int sockfd;
	sockfd = fly_socket_init((char *) "127.0.0.1", 10080, FLY_IP_V4);
	EXPECT_TRUE(sockfd > 0);
	EXPECT_TRUE(fly_socket_release(sockfd) != -1);
}
