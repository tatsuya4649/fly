#include <sys/socket.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "fly.h"
#include "server.h"

#define FLY_IP_V4			4
#define FLY_IP_V6			6
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
		return -1;
	}

	sockfd = socket(fly_ip, SOCK_STREAM, 0);
    if (sockfd == -1)
        return -1;

    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		return -1;

    memset(&bind_addr, 0, sizeof(struct sockaddr_storage));

	switch(fly_ip){
	/* IPv4 */
	case AF_INET:
		{
			struct sockaddr_in *in = (struct sockaddr_in *) &bind_addr;
			in->sin_family = fly_ip;
			if (inet_pton(fly_ip, host, &in->sin_addr.s_addr) != 1)
				return -1;
			in->sin_port = htons((unsigned short) port);

			if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(struct sockaddr_in)) == -1){
				close(sockfd);
				return -1;
			}
		}
		break;
	/* IPv6 */
	case AF_INET6:
		{
			struct sockaddr_in6 *in6 = (struct sockaddr_in6 *) &bind_addr;
			in6->sin6_family = fly_ip;
			if (inet_pton(fly_ip, host, &in6->sin6_addr.s6_addr) != 1)
				return -1;
			in6->sin6_port = htons((unsigned short) port);

			if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(struct sockaddr_in)) == -1){
				close(sockfd);
				return -1;
			}
		}
		break;
	default:
		return -1;
	}
    if (listen(sockfd, (int) FLY_BACK_LOG) == -1){
        close(sockfd);
        return -1;
    }
	return sockfd;
}

int fly_socket_release(int sockfd)
{
	return close(sockfd);
}
