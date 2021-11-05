#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "fly.h"
#include "server.h"
#include "err.h"
#include "ssl.h"
#include "conf.h"

int fly_socket_nonblocking(fly_sock_t s)
{
	int val = 1;
	return ioctl(s, FIONBIO, &val);
}

int fly_backlog(void)
{
	return fly_config_value_int(FLY_BACKLOG);
}

void fly_add_sockinfo(fly_context_t *ctx, fly_sockinfo_t *info)
{
	ctx->listen_sock = info;
	ctx->listen_count++;
	return;
}

int fly_socket_init(fly_context_t *ctx, int port, fly_sockinfo_t *info, int flag){

	if (!info)
		return -1;

	fly_sock_t sockfd;
	int res;
    int option = FLY_SOCKET_OPTION;
	struct addrinfo hints;
	struct addrinfo *result, *rp;
	char port_str[FLY_PORTSTR_LEN];

	memset(&hints, 0, sizeof(struct addrinfo));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_protocol = 0;
	hints.ai_flags = AI_PASSIVE;

	res = snprintf(port_str, FLY_PORTSTR_LEN, "%d", port);
	if (res <= 0 || res >= FLY_PORTSTR_LEN)
		return -1;

	if (getaddrinfo(NULL, port_str, &hints, &result) != 0)
		return -1;

	for (rp=result; rp!=NULL; rp=rp->ai_next){
		sockfd = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
		if (sockfd == -1)
			continue;

		if (fly_socket_nonblocking(sockfd) == -1)
			continue;
		if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
			goto error;

		if (bind(sockfd, rp->ai_addr, rp->ai_addrlen) == 0)
			break;

		close(sockfd);
	}

	/* can't bind to port */
	if (rp == NULL)
		return -1;

	if (getnameinfo((const struct sockaddr *) rp->ai_addr, (socklen_t) rp->ai_addrlen, info->hostname, NI_MAXHOST, info->servname, NI_MAXSERV, FLY_LISTEN_SOCKINFO_FLAG) != 0)
		goto error;
	if (listen(sockfd, fly_backlog()) == -1)
		goto error;

	info->fd = sockfd;
	memcpy(&info->addr, rp->ai_addr, rp->ai_addrlen);
	info->addrlen = rp->ai_addrlen;
	info->flag = flag;
	if (info->flag & FLY_SOCKINFO_SSL){
		char *crt_path_env = fly_ssl_crt_path();
		char *key_path_env = fly_ssl_key_path();
		if (!crt_path_env || !key_path_env)
			return -1;

		info->crt_path = crt_path_env ? fly_pballoc(ctx->pool, sizeof(char)*(strlen(crt_path_env)+1)) : NULL;
		info->key_path = key_path_env ? fly_pballoc(ctx->pool, sizeof(char)*(strlen(key_path_env)+1)) : NULL;
		if (info->crt_path){
			memset(info->crt_path, '\0', sizeof(char)*(strlen(crt_path_env)+1));
			memcpy(info->crt_path, crt_path_env, sizeof(char)*strlen(crt_path_env));
		}
		if (info->key_path){
			memset(info->key_path, '\0', sizeof(char)*(strlen(key_path_env)+1));
			memcpy(info->key_path, key_path_env, sizeof(char)*strlen(key_path_env));
		}
	}
	freeaddrinfo(result);
	fly_add_sockinfo(ctx, info);
	return sockfd;
error:
	close(sockfd);
	freeaddrinfo(result);
	return FLY_EMAKESOCK;
}

int fly_socket_release(int sockfd)
{
	return close(sockfd);
}

char *fly_server_host(void)
{
	return fly_config_value_str(FLY_HOST);
}

int fly_server_port(void)
{
	return fly_config_value_int(FLY_PORT);
}

int fly_socket_close(int fd, int how __unused)
{
	return close (fd);
}
