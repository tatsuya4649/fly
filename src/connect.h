#ifndef _CONNECT_H
#define _CONNECT_H

#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/ssl.h>
#include "alloc.h"
#include "event.h"
#include "version.h"
#include "buffer.h"


#define FLY_CONNECTION_POOL_SIZE		1
struct fly_hv2_state;
struct fly_buffer;
struct fly_connect{
	fly_event_t				*event;
	int						sockfd;
	int						c_sockfd;
	fly_pool_t				*pool;
	char					hostname[NI_MAXHOST];
	char 					servname[NI_MAXSERV];
	struct sockaddr_storage peer_addr;
	socklen_t				addrlen;
	/* for connection buffer */
	size_t					buffer_init_len;
	size_t					buffer_per_len;
	/* for ssl/tls connection */
	SSL						*ssl;
	SSL_CTX					*ssl_ctx;
	fly_http_version_t		*http_v;
#define FLY_SSL_CONNECT			(1<<0)
	int						flag;

	/* HTTP2 */
	struct fly_hv2_state	*v2_state;
	struct fly_buffer		*buffer;

	fly_bit_t				peer_closed: 1;
};
typedef struct fly_connect fly_connect_t;

fly_connect_t *fly_connect_init(int sockfd, int c_sockfd, fly_event_t *event, struct sockaddr *addr, socklen_t addrlen);
void fly_connect_buffer_refresh(fly_connect_t *conn);
int fly_connect_release(fly_connect_t *conn);
int fly_info_of_connect(fly_connect_t *conn);
int fly_connect_recv(fly_connect_t *conn);
int fly_listen_connected(fly_event_t *e);
int fly_accept_listen_socket_handler(struct fly_event *event);
int fly_listen_socket_end_handler(fly_event_t *__e);

#define FLY_CONNECT_HTTP_VERSION(conn)		\
			(conn->http_v->type)

#define FLY_MAX_REQUEST_LENGTH		"FLY_MAX_REQUEST_LENGTH"
size_t fly_max_request_length(void);

#define FLY_CONNECT_BUFFER_INIT_LEN "FLY_CONNECT_BUFFER_INIT_LEN"
#define FLY_CONNECT_BUFFER_PER_LEN	"FLY_CONNECT_BUFFER_PER_LEN"
size_t fly_connect_buffer_init_len(void);
size_t fly_connect_buffer_per_len(void);

#endif
