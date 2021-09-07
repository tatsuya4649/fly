#include <assert.h>
#include "server.h"

#define TEST_PORT				1234
int main()
{
	fly_sockinfo_t info;
	int res;
	res = fly_socket_init(TEST_PORT, &info);

	assert(res > 0);
	assert(fly_socket_release(info.sockfd) != -1);
}
