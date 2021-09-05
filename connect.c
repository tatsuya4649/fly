#include "connect.h"
#include <stdio.h>
#include <sys/socket.h>

fly_connect_t *fly_connect_init(int sockfd)
{
	fly_pool_t *pool;
	fly_connect_t *conn;
	pool = fly_create_pool(FLY_CONNECTION_POOL_SIZE);
	conn = fly_pballoc(pool, sizeof(fly_connect_t));
	conn->sockfd = sockfd;
	conn->pool = pool;
	conn->addrlen = sizeof(conn->client_addr);
	if (conn == NULL)
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

int fly_connect_accept(fly_connect_t *conn)
{
	if (conn == NULL)
		return -1;

	int c_sockfd;
	c_sockfd = accept(conn->sockfd, (struct sockaddr *) &conn->client_addr, &conn->addrlen);
	if (c_sockfd == -1){
		return -1;
	}
	conn->c_sockfd = c_sockfd;
	return 0;
}

int fly_info_of_connect(fly_connect_t *conn)
{
	int gname_err;
	gname_err=getnameinfo(
		(struct sockaddr *) &conn->client_addr,
		conn->addrlen,
		conn->hostname,
		NI_MAXHOST,
		conn->servname,
		NI_MAXSERV,
		NI_NUMERICHOST
	);
	if (gname_err != 0){
		fprintf(stderr, "getnameinfo: %s\n", gai_strerror(gname_err));
		return -1;
	}
	fprintf(stderr, "Peer Info: %s\n", conn->hostname);
	return 0;
}
