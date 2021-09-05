#ifndef _SERVER_H
#define _SERVER_H

#include "version.h"
#include "method.h"
#include "request.h"

#define FLY_IP_V4			4
#define FLY_IP_V6			6

#define FLY_SOCKET_OPTION		1
int fly_socket_init(char *host, int port, int ip_v);
int fly_socket_release(int sockfd);

#endif
