#ifndef _SSL_H
#define _SSL_H

#include <openssl/ssl.h>
#include <openssl/err.h>
#include "context.h"
#include "event.h"
#include "conf.h"
#include "connect.h"

/*
 *	whether to use ssl/tls.
 */
#define FLY_SSL					"FLY_SSL"
/*
 * environment variable of certificate file.
 * used in ssl/tls connection.
 */
#define FLY_SSL_CRT_PATH		"FLY_SSL_CRT_PATH"

/*
 * environment variable of private file.
 * used in ssl/tls connection.
 */
#define FLY_SSL_KEY_PATH		"FLY_SSL_KEY_PATH"

int fly_accept_listen_socket_ssl_handler(fly_event_t *e, fly_connect_t *conn);
struct fly_connect;
void fly_ssl_connected_release(struct fly_connect *conn);
int fly_ssl_error_log(fly_event_manager_t *manager);
void fly_listen_socket_ssl_setting(fly_context_t *ctx, fly_sockinfo_t *sockinfo);

bool fly_ssl(void);
char *fly_ssl_crt_path(void);
char *fly_ssl_key_path(void);

#define FLY_TLS_HANDSHAKE_MAGIC				(0x16)

static inline bool fly_tls_handshake_magic(char *b)
{
	return *b == FLY_TLS_HANDSHAKE_MAGIC ? true : false;
}

__fly_noreturn void FLY_SSL_EMERGENCY_ERROR(fly_context_t *ctx);
#endif
