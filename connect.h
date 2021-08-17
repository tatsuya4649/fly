#include <netinet/in.h>

typedef struct {
	int c_sock;
	char *hostname;
	char *servname;
	struct sockaddr_storage *client_addr;
} http_connection;

