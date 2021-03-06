#ifndef _SERVER_H
#define _SERVER_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#include <unistd.h>
#include <stdlib.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <netdb.h>
#include "version.h"
#include "err.h"
#include "method.h"

#define FLY_IP_V4			4
#define FLY_IP_V6			6

typedef int fly_sock_t;
struct fly_sockinfo{
	fly_sock_t				fd;
	struct sockaddr_storage addr;
	socklen_t				addrlen;
	char					hostname[NI_MAXHOST];
	char 					servname[NI_MAXSERV];
	/* for ssl */
	char					*crt_path;
	char 					*key_path;

#define FLY_SOCKINFO_SSL		(1<<0)
	int						flag;
};
typedef struct fly_sockinfo fly_sockinfo_t;
#define FLY_SOCKET_OPTION		1
struct fly_context;
struct fly_err;
int fly_socket_init(fly_context_t *ctx, int port, fly_sockinfo_t *info, int flag, struct fly_err *err);
int fly_socket_release(int sockfd);

#define FLY_PORT				"FLY_PORT"
#define FLY_HOST				"FLY_HOST"
#define FLY_BACKLOG				"FLY_BACKLOG"
#define FLY_PORTSTR_LEN			100
#define FLY_LISTEN_SOCKINFO_FLAG		(NI_NUMERICSERV)
const char *fly_sockport_env(void);
int fly_socket_nonblocking(fly_sock_t s);
#define FLY_SOCK_READ_CLOSE				(SHUT_RD)
#define FLY_SOCK_WRTITE_CLOSE			(SHUT_WR)
#define FLY_SOCK_CLOSE					(SHUT_RDWR)
int fly_socket_close(int fd, int how);
int fly_server_port(void);
char *fly_server_host(void);
long fly_backlog(void);

#ifdef DEBUG
int fly_can_recv(int fd);
#endif

#endif
