#include "connect.h"
#include <stdio.h>
#include <sys/socket.h>

__fly_static int __fly_info_of_connect(fly_connect_t *conn);

fly_connect_t *fly_connect_init(int sockfd, int c_sockfd, fly_event_t *event, struct sockaddr *addr, socklen_t addrlen)
{
	fly_pool_t *pool;
	fly_connect_t *conn;

	pool = fly_create_pool(FLY_CONNECTION_POOL_SIZE);
	conn = fly_pballoc(pool, sizeof(fly_connect_t));
	if (conn == NULL)
		return NULL;

	conn->event = event;
	conn->sockfd = sockfd;
	conn->c_sockfd = c_sockfd;
	conn->pool = pool;
	memcpy(&conn->peer_addr, addr, addrlen);
	conn->addrlen = addrlen;

	if (__fly_info_of_connect(conn) == -1)
		return NULL;

	return conn;
}

int fly_connect_release(fly_connect_t *conn)
{
	if (conn == NULL)
		return -1;
	close(conn->c_sockfd);
	return fly_delete_pool(&conn->pool);
}

__fly_static int __fly_info_of_connect(fly_connect_t *conn)
{
	int gname_err;
	gname_err=getnameinfo(
		(struct sockaddr *) &conn->peer_addr,
		conn->addrlen,
		conn->hostname,
		NI_MAXHOST,
		conn->servname,
		NI_MAXSERV,
		NI_NUMERICHOST
	);
	if (gname_err != 0){
		return -1;
	}
	return 0;
}
