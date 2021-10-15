#ifndef _SSL_H
#define _SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "event.h"

/*
 *	whether to use ssl/tls.
 */
#define FLY_SSL_USE_ENV				"FLY_SSL_USE"
/*
 * environment variable of certificate file.
 * used in ssl/tls connection.
 */
#define FLY_SSL_CRT_PATH_ENV		"FLY_SSL_CRT_PATH"

/*
 * environment variable of private file.
 * used in ssl/tls connection.
 */
#define FLY_SSL_KEY_PATH_ENV		"FLY_SSL_KEY_PATH"

int fly_listen_socket_ssl_handler(fly_event_t *e);
struct fly_connect;
void fly_ssl_connected_release(struct fly_connect *conn);

#endif
