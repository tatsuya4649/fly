#ifndef _CONNECT_H
#define _CONNECT_H

#include <netinet/in.h>
#include "alloc.h"

typedef struct {
	int c_sock;
	char *hostname;
	char *servname;
	struct sockaddr_storage *client_addr;
	fly_pool_t *pool;
} http_connection;

#endif
