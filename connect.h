#ifndef _CONNECT_H
#define _CONNECT_H

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include "alloc.h"


#define FLY_CONNECTION_POOL_SIZE		1
struct fly_connect{
	int sockfd;
	int c_sockfd;
	fly_pool_t *pool;
	char hostname[NI_MAXHOST];
	char servname[NI_MAXSERV];
	struct sockaddr_storage client_addr;
	socklen_t addrlen;
};
typedef struct fly_connect fly_connect_t;

fly_connect_t *fly_connect_init(int sockfd);
int fly_connect_release(fly_connect_t *conn);
int fly_connect_accept(fly_connect_t *conn);
int fly_info_of_connect(fly_connect_t *conn);

#endif
