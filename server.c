#include <sys/socket.h>
#include <netdb.h>
#include <arpa/inet.h>
#include "fly.h"
#include "server.h"

int fly_socket_init()
{
	int sockfd;
    int option = FLY_SOCKET_OPTION;
    struct sockaddr_in bind_addr;

    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option)) == -1)
		return -1;
    if (sockfd == -1)
        return -1;

    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = AF_INET;
    inet_pton(AF_INET, FLY_SERV_ADDR, &bind_addr.sin_addr.s_addr);
    bind_addr.sin_port = htons((unsigned short) FLY_BIND_PORT);

    if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(bind_addr)) == -1){
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, (int) FLY_BACK_LOG) == -1){
        close(sockfd);
        return -1;
    }
	return sockfd;
}


