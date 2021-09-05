#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <errno.h>
#include "fly.h"
#include "server.h"
#include "err.h"

int fly_socket_init(
	__unused char *host,
	__unused int port,
	__unused int ip_v
){
	int sockfd, fly_ip;
    int option = FLY_SOCKET_OPTION;
    struct sockaddr_storage bind_addr;

	switch(ip_v){
	case FLY_IP_V4:
		fly_ip = AF_INET;
		break;
	case FLY_IP_V6:
		fly_ip = AF_INET6;
		break;
	default:
		return FLY_EUNKNOWN_IP;
	}

	sockfd = socket(fly_ip, SOCK_STREAM, 0);
    if (sockfd == -1)
        return FLY_EMAKESOCK;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		return FLY_ESETOPTSOCK;

    memset(&bind_addr, 0, sizeof(struct sockaddr_storage));

	switch(fly_ip){
	/* IPv4 */
	case AF_INET:
		{
			struct sockaddr_in *in = (struct sockaddr_in *) &bind_addr;
			in->sin_family = fly_ip;
			switch (inet_pton(fly_ip, host, &in->sin_addr.s_addr)){
			case 1:
				break;
			case 0:
				return FLY_EINCADDR;
			default:
				return FLY_ECONVNET;
			}
			in->sin_port = htons((unsigned short) port);

			if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(struct sockaddr_in)) == -1){
				int bind_err;
				switch(errno){
				case EACCES:
					bind_err = FLY_EBINDSOCK_ACCESS;
					goto bind_err_v4;
				case EADDRINUSE:
					bind_err = FLY_EBINDSOCK_ADDRINUSE;
					goto bind_err_v4;
				case EBADF:
					bind_err = FLY_EBINDSOCK_BADF;
					goto bind_err_v4;
				case EINVAL:
					bind_err = FLY_EBINDSOCK_INVAL;
					goto bind_err_v4;
				case ENOTSOCK:
					bind_err = FLY_EBINDSOCK_NOTSOCK;
					goto bind_err_v4;
				default:
					bind_err = FLY_EBINDSOCK;
					goto bind_err_v4;
				}
bind_err_v4:
				close(sockfd);
				return bind_err;
			}
		}
		break;
	/* IPv6 */
	case AF_INET6:
		{
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &bind_addr;
			in6->sin6_family = fly_ip;
			if (inet_pton(fly_ip, host, &in6->sin6_addr.s6_addr) != 1)
				return FLY_ECONVNET;
			in6->sin6_port = htons((unsigned short) port);

			if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(struct sockaddr_in)) == -1){
				int bind_err;
				switch(errno){
				case EACCES:
					bind_err = FLY_EBINDSOCK_ACCESS;
					goto bind_err_v6;
				case EADDRINUSE:
					bind_err = FLY_EBINDSOCK_ADDRINUSE;
					goto bind_err_v6;
				case EBADF:
					bind_err = FLY_EBINDSOCK_BADF;
					goto bind_err_v6;
				case EINVAL:
					bind_err = FLY_EBINDSOCK_INVAL;
					goto bind_err_v6;
				case ENOTSOCK:
					bind_err = FLY_EBINDSOCK_NOTSOCK;
					goto bind_err_v6;
				default:
					bind_err = FLY_EBINDSOCK;
					goto bind_err_v6;
				}
bind_err_v6:
				close(sockfd);
				return bind_err;
			}
		}
		break;
	default:
		return FLY_EUNKNOWN_IP;
	}
    if (listen(sockfd, (int) FLY_BACK_LOG) == -1){
        close(sockfd);
        return FLY_ELISTSOCK;
    }
	return sockfd;
}

int fly_socket_release(int sockfd)
{
	return close(sockfd);
}
