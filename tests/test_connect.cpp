#ifdef __cplusplus
extern "C"{
	#include "server.h"
	#include "connect.h"
}
#endif
#include <unistd.h>
#include <sys/socket.h>
#include <signal.h>
#include <netdb.h>
#include <gtest/gtest.h>
#include <sys/wait.h>

TEST(CONNECT, fly_connect_init)
{
	EXPECT_TRUE(fly_connect_init(0) != NULL);
}

TEST(CONNECT, fly_connect_release)
{
	fly_connect_t *con;
	EXPECT_TRUE((con=fly_connect_init(0)) != NULL);
	EXPECT_TRUE(fly_connect_release(con) == 0);
	EXPECT_TRUE(fly_connect_release(NULL) == -1);
}

TEST(CONNECT, fly_connect_accept)
{
//	fly_connect_t *con;
//	int sockfd;
//	pid_t cpid;
//	
//	sockfd = fly_socket_init();
//	EXPECT_TRUE(sockfd != -1);
//	EXPECT_TRUE((con=fly_connect_init(sockfd)) != NULL);
//	
//	EXPECT_TRUE(fly_connect_accept(NULL) == -1);
//	
//
//	switch(cpid=fork()){
//	case 0:
//		/* child */
//		EXPECT_TRUE(fly_connect_accept(con) == 0);
//		return;
//	case -1:
//		break;
//	default:
//		/* parent */
//		int c_sockfd = socket(AF_INET, SOCK_STREAM, 0);
//		struct addrinfo *info;
//		if (c_sockfd == -1)
//			goto end;
//
//		if (getaddrinfo("localhsot", "3333", NULL, &info) == -1)
//			goto end;
//
//		sleep(3);
//		EXPECT_TRUE(connect(c_sockfd, info->ai_addr, info->ai_addrlen) != -1);
//
//		waitpid(cpid, NULL, 0);
//end:
//		break;
//	}
//	EXPECT_TRUE(fly_socket_release(sockfd) != -1);
}
