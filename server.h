#ifndef _SERVER_H
#define _SERVER_H

#include <stdlib.h>
#include <sys/ioctl.h>
#include "version.h"
#include "method.h"
#include "request.h"

#define FLY_IP_V4			4
#define FLY_IP_V6			6

typedef int fly_sock_t;
struct fly_sockinfo{
	fly_sock_t fd;
	struct sockaddr_storage addr;
	socklen_t addrlen;
};
typedef struct fly_sockinfo fly_sockinfo_t;
#define FLY_SOCKET_OPTION		1
int fly_socket_init(int port, fly_sockinfo_t *info);
int fly_socket_release(int sockfd);

#define FLY_PORT_ENV			"FLY_PORT"
#define FLY_BACKLOG_DEFAULT		1024
#define FLY_PORTSTR_LEN			100
const char *fly_sockport_env(void);
int fly_socket_nonblocking(fly_sock_t s);

#endif
