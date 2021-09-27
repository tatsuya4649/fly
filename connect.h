#ifndef _CONNECT_H
#define _CONNECT_H

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include "alloc.h"
#include "event.h"
#include "version.h"


#define FLY_CONNECTION_POOL_SIZE		1
struct fly_connect{
	fly_event_t *event;
	int sockfd;
	int c_sockfd;
	fly_pool_t *pool;
	char hostname[NI_MAXHOST];
	char servname[NI_MAXSERV];
	struct sockaddr_storage peer_addr;
	socklen_t addrlen;
	/* for ssl/tls connection */
	SSL *ssl;
	SSL_CTX *ssl_ctx;
	fly_http_version_t *http_v;
#define FLY_SSL_CONNECT			(1<<0)
	int flag;
};
typedef struct fly_connect fly_connect_t;

fly_connect_t *fly_connect_init(int sockfd, int c_sockfd, fly_event_t *event, struct sockaddr *addr, socklen_t addrlen);
int fly_connect_release(fly_connect_t *conn);
int fly_info_of_connect(fly_connect_t *conn);
int fly_connect_recv(fly_connect_t *conn);
int fly_listen_connected(fly_event_t *e);

#endif
