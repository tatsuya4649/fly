#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <ctype.h>

#include "fly.h"
#include "server.h"
#include "response.h"
#include "connect.h"
#include "header.h"
#include "alloc.h"
#include "fs.h"
#include "request.h"
#include "header.h"
#include "body.h"
#include "api.h"
#include "fsignal.h"

fly_response_t *test_function(__unused fly_request_t *request);

int main()
{
	int sockfd;
	/* socket setting */
	if ((sockfd = fly_socket_init()) == -1){
		perror("fly_socket_init");
		return -1;
	}
    /* signal setting */
    if (fly_signal_init() == -1){
        perror("fly_signal_init");
        close(sockfd);
        return -1;
    }

	/* mount point setting */
	if (fly_fs_init() == -1){
		perror("fly_fs_init");
		return -1;
	}
	if (fly_fs_mount(".") == -1){
		perror("fly_fs_mount");
		return -1;
	}

	/* register init */
	fly_route_reg_t *reg;
	if (fly_route_init() == -1){
		perror("fly_route_init");
		return -1;
	}
	if ((reg=fly_route_reg_init()) == NULL){
		perror("fly_route_reg_init");
		return -1;
	}
	/* register route */
	if (fly_register_route(reg, test_function, "/", GET) == -1){
		perror("fly_register_route");
		return -1;
	}

    while (1){
        fprintf(stderr, "Waiting Connection...\n");
		/* connection init */
		fly_connect_t *conn;
		conn = fly_connect_init(sockfd);
		if (conn == NULL)
			goto error;
		/* accept */
		if (fly_connect_accept(conn) == -1)
			goto error;
		/* request setting */
        fly_request_t *req;
        req = fly_request_init();
		if (req == NULL)
			goto error;
        fprintf(stderr, "Connected\n");
		req->connect = conn;

		/* waiting for request... */
        int recv_len;
        recv_len = recv(conn->c_sockfd, req->buffer, FLY_BUFSIZE, 0);
        if (recv_len == 0)
            goto end_connection;
        else if (recv_len == -1)
            goto error;
		/* get peer info */
		if (fly_info_of_connect(conn) == -1)
			goto error;
        /* request operation*/
        /* get request_line */
        fly_reqlinec_t *request_line = fly_get_request_line_ptr(req->buffer);
        if (fly_request_operation(conn->c_sockfd, req->pool, request_line, req) < 0)
            goto error;
        /* get header */
        char *header_lines = fly_get_header_lines_ptr(req->buffer);
        if (fly_reqheader_operation(req, header_lines) == -1){
			fly_500_error(conn->c_sockfd, req->request_line->version->type);
			goto error;
		} /* get body */
		fly_body_t *body = fly_body_init();
		req->body = body;
        fly_bodyc_t *body_ptr = fly_get_body_ptr(req->buffer);
		if (fly_body_setting(body, body_ptr) == -1)
			goto error;
        printf("BODY: %s\n",req->body->body);

		fly_route_t *route;
		__unused fly_response_t *response;
		route = fly_found_route(reg, req->request_line->uri.uri, req->request_line->method->type);

		if (route == NULL)
			fly_404_error(req->connect->c_sockfd, req->request_line->version->type);
		else{
			/* found route */
			response = route->function(req);
		}

		fly_response(req->connect->c_sockfd, response, 0);

		fly_request_release(req);
		fly_connect_release(conn);
		fly_route_release();
end_connection:
		fly_request_release(req);
		fly_connect_release(conn);
		fly_route_release();
        continue;
error:
		fly_request_release(req);
		fly_connect_release(conn);
		fly_route_release();
        continue;
    }

    /* end of server */
	fly_fs_release();
	fly_socket_release(sockfd);
    return 0;
}
