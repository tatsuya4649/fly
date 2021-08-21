#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <signal.h>
#include <ctype.h>

#include "server.h"
#include "response.h"
#include "connect.h"
#include "header.h"
#include "alloc.h"
#include "fs.h"
#include "request.h"
#include "header.h"

#define BIND_PORT       3333
#define BACK_LOG        4096
#define SERV_ADDR       "127.0.0.1"
#define BUF_SIZE        (8000*8)
#define HEADER_LINES    100

void sigint_handler(__unused int signo)
{
    fprintf(stderr, "Interrupt now (Ctrl+C)...");
	fly_fs_release();
    exit(0);
}

char *strtokc(char *s1, char *s2)
{
    static char *str = 0;
    if (s1){
        str = s1;
    }else{
        s1 = str;
    }
    if (!s1) return NULL;

    while (1){
        if (!*str){
            str = 0;
            return s1;
        }

        if (*str == *s2){
            *str++ = 0;
            return s1;
        }

        str++;
    }
}


fly_hdr_t *fly_header_con(fly_hdr_t *h1, fly_hdr_t *h2)
{
	fly_hdr_t *ptr;
	for (ptr=h1; ptr->next!=NULL; ptr=ptr->next)
		;
	ptr->next = h2;
	return h1;
}
int fly_headers_length(fly_hdr_t *hdrs)
{
	int i=0;
	fly_hdr_t *ptr;
	
	for (ptr=hdrs; ptr!=NULL; ptr=ptr->next)
		i++;
	return i;
}

http_request *alloc_request(fly_pool_t *pool)
{
    /* request content */
    http_request *req = fly_palloc(pool, fly_page_convert(sizeof(http_request)));
    request_info *reqinfo = fly_palloc(pool, fly_page_convert(sizeof(request_info)));
    reqinfo->version = fly_palloc(pool, fly_page_convert(sizeof(http_version_t)));
    req->rinfo = reqinfo;
    return req;
}

int fly_header_operation(fly_pool_t *pool, char *headers, http_request *req)
{
    char *header_line = headers;
    char *header_line_end;
    char **header_lines = fly_pballoc(pool, sizeof(char *)*HEADER_LINES);
	if (header_lines == NULL)
		return -1;

    int i = 0;
    while (1){
		if (i == HEADER_LINES)
			return -1;
        header_line_end = strstr(header_line, "\r\n");
        if (header_line_end-header_line == 0)
            break;
        header_lines[i] = (char *) fly_pballoc(pool, sizeof(char)*(header_line_end-header_line+1));
		if (header_lines[i] == NULL)
			return -1;
        memcpy(header_lines[i], header_line, header_line_end-header_line);
        header_lines[i][header_line_end-header_line] = '\0';

        i++;
        /* next line */
        header_line = header_line_end + CRLF_LENGTH;
    }
    header_lines[i+1] = NULL;
    req->header_lines = header_lines;
    req->header_len = i;

    for (int j=0; j<req->header_len; j++){
        printf("HEADER_Line: %s\n", header_lines[j]);
    }
	return 0;
}

char *get_request_line_point(char *buffer)
{
    return buffer;
}

char *get_header_lines_point(char *buffer)
{
    return strstr(buffer, "\r\n") + CRLF_LENGTH;
}

char *get_body_point(char *buffer)
{
    char *newline_point;
    newline_point = strstr(buffer, "\r\n\r\n");
    if (newline_point != NULL)
        return newline_point + 2*CRLF_LENGTH;
    return NULL;
}

int main()
{
    int sockfd, c_sock;
    int option = 1;
    struct sockaddr_in bind_addr;
    struct sigaction intact;
    sockfd = socket(AF_INET, SOCK_STREAM, 0);
    setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &option, sizeof(option));
    if (sockfd == -1){
        perror("socket");
        return -1;
    }

    memset(&bind_addr, 0, sizeof(struct sockaddr_in));
    bind_addr.sin_family = AF_INET;
    inet_pton(AF_INET, SERV_ADDR, &bind_addr.sin_addr.s_addr);
    bind_addr.sin_port = htons((unsigned short) BIND_PORT);

    if (bind(sockfd, (const struct sockaddr *) &bind_addr, sizeof(bind_addr)) == -1){
        perror("bind");
        close(sockfd);
        return -1;
    }

    if (listen(sockfd, (int) BACK_LOG) == -1){
        perror("listen");
        close(sockfd);
        return -1;
    }

    /* signal setting */
    intact.sa_handler = sigint_handler;
    intact.sa_flags = 0;
    if (sigaction(SIGINT, &intact,NULL) == -1){
        perror("sigaction");
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

    while (1){
        fprintf(stderr, "Waiting Connection...\n");
        socklen_t addrlen;
        struct sockaddr_storage client_addr;
        char hostname[NI_MAXHOST];
        char servname[NI_MAXSERV];
        int gname_err;
        addrlen = sizeof(client_addr);
		/* accept */
        c_sock = accept(sockfd, (struct sockaddr *) &client_addr, &addrlen);
		/* make pool */
		fly_pool_t *pool;
		pool = fly_create_pool(FLY_DEFAULT_ALLOC_PAGESIZE);
        int recv_len;
        char *buffer = fly_pballoc(pool, BUF_SIZE);
		if (buffer == NULL){
			goto error;
		}
        if (c_sock == -1){
            perror("accept");
            continue;
        }
        fprintf(stderr, "Connected\n");
        gname_err=getnameinfo(
            (struct sockaddr *) &client_addr,
            addrlen,
            hostname,
            sizeof(hostname),
            NULL,
            0,
            NI_NUMERICHOST
        );
        if (gname_err != 0){
            fprintf(stderr, "getnameinfo: %s\n", gai_strerror(gname_err));
            goto error;
        }
        fprintf(stderr, "Peer Info: %s\n", hostname); 
		memset(buffer, 0, BUF_SIZE);
        recv_len = recv(c_sock, buffer, BUF_SIZE, 0);
        if (recv_len == 0){
            goto end_connection;
        }else if (recv_len == -1){
            goto error;
        }
        /* connection */
        http_connection conn;
        conn.c_sock = c_sock;
        conn.hostname = hostname;
        conn.servname = servname;
        conn.client_addr = &client_addr;
		conn.pool = pool;
        /* request operation*/
        http_request *req;
        req = alloc_request(pool);
        /* get request_line */
        char *request_line = get_request_line_point(buffer);
        if (fly_request_operation(conn.c_sock, pool, request_line, req) < 0){
            goto error;
        }
        /* get header */
        char *header_lines = get_header_lines_point(buffer);
        if (fly_header_operation(pool, header_lines, req) == -1){
			fly_500_error(c_sock, req->rinfo->version->type);
			goto error;
		}
        /* get body */
        char *body = get_body_point(buffer);;
        req->body = body;
        printf("BODY: %s\n",body);

		//__unused char *res = fly_from_path(pool, XS, FLY_FS_INIT_NUMBER, "test");
		__unused fly_hdr_ci *ci = fly_header_init();
		if (fly_header_add(ci, "Content-Length", "11") == -1)
			goto error;
		if (fly_header_add(ci, "Connection", "close") == -1)
			goto error;
		printf("%s\n",fly_header_from_chain(ci));
		char *header = fly_header_from_chain(ci);
		fly_response(
			c_sock,
			pool,
			_200,
			V1_1,
			header,
			strlen(header),
			"Hello World",
			strlen("Hello World"),
			0
		);

		fly_header_release(ci);

		fly_delete_pool(pool);
        close(c_sock);
end_connection:
		fly_delete_pool(pool);
        close(c_sock);
        continue;
error:
		fly_delete_pool(pool);
        close(c_sock);
        continue;
    }

    /* end of server */
	fly_fs_release();
    close(sockfd);
    return 0;
}
