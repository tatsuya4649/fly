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

#define BIND_PORT       3333
#define BACK_LOG        4096
#define SERV_ADDR       "127.0.0.1"
#define BUF_SIZE        (8000*8)
#define HEADER_LINES    100

void sigint_handler(__unused int signo)
{
    fprintf(stderr, "Interrupt now (Ctrl+C)...");
	fly_hdr_release();
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


http_method *match_method(char *method_name)
{
    /* method name should be lower */
    for (char *n=method_name; *n!='\0'; n++)
        *n = tolower((int) *n);

    for (http_method *type=methods; type->name!=NULL; type++){
        if (strcmp(method_name, type->name) == 0)
            return type;
    }
    return NULL;
}

http_version *match_version(char *version)
{
    /* version name should be upper */
    for (char *n=version; *n!='\0'; n++)
        *n = toupper((int) *n);

    for (http_version *ver=versions; ver->full!=NULL; ver++){
        if (strcmp(version, ver->full) == 0)
            return ver;
    }
    return NULL;
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
fly_hdr_t *fly_response_err(fly_pool_t *pool)
{
	fly_hdr_t *headers;
	headers = fly_pballoc(pool, sizeof(fly_hdr_t));
	strcpy(headers->name, "connection");
	headers->trig = fly_connection_close_header;
	headers->next = NULL;
	return headers;
}
void nothing_uri(int c_sock)
{
	fly_pool_t *respool;
	fly_hdr_t *headers;

	respool = fly_response_init();
	headers = fly_response_err(respool);
    fly_response(
		c_sock,
		respool,
		_400,
		NULL,
		headers,
		fly_headers_length(headers),
		fly_code_explain(_400),
		strlen(fly_code_explain(_400))	
	);
	fly_response_release(respool);
}
void unmatch_method(int c_sock)
{
	fly_pool_t *respool;
	fly_hdr_t *headers;

	respool = fly_response_init();
	headers = fly_response_err(respool);
    fly_response(
		c_sock,
		respool,
		_400,
		NULL,
		headers,
		fly_headers_length(headers),
		NULL,
		0
	);
	fly_response_release(respool);
}

void no_version(int c_sock)
{
	fly_pool_t *respool;
	fly_hdr_t *headers;

	respool = fly_response_init();
	headers = fly_response_err(respool);
    fly_response(
		c_sock,
		respool,
		_400,
		NULL,
		headers,
		fly_headers_length(headers),
		NULL,
		0
	);
	fly_response_release(respool);
}

int parse_request_line(fly_pool_t *pool, int c_sock, char *request_line, request_info *req)
{
    char *method;
    char *space;
    printf("%s\n", request_line);
    space = strstr(request_line, " ");
    /* method only => response 400 Bad Request */
    if (space == NULL){
        nothing_uri(c_sock);
        return -1;
    }
    method = fly_palloc(pool, fly_page_convert((space-request_line)+1));
	/* memory alloc error */
	if (method == NULL){
		return -1;
	}
    memcpy(method, request_line, space-request_line);
    method[space-request_line] = '\0';
    req->method = match_method(method);
	/* no match method */
    if (req->method == NULL){
        return -1;
	}

    char *uri_start = space+1;
    char *next_space = strstr(uri_start," ");
	/* no version */
	if (next_space == NULL){
		no_version(c_sock);
		return -1;
	}
    req->uri = fly_palloc(pool, fly_page_convert(next_space-uri_start+1));
	if (req->uri == NULL){
		return -1;
	}
    memcpy(req->uri, space+1, next_space-uri_start);
    req->uri[next_space-uri_start] = '\0';
    printf("%s\n", req->uri);

    char *version_start = next_space+1;
    req->version = match_version(version_start);
    if (req->version == NULL){
        printf("NOT FOUND VERSION\n");
    }

    printf("%s\n", req->version->full);
    char *slash_p = strchr(req->version->full,'/');
    if (slash_p == NULL)
        return -1;

    req->version->number = slash_p + 1;
    printf("%s\n", req->version->number);
    return 0;
}

http_request *alloc_request(fly_pool_t *pool)
{
    /* request content */
    http_request *req = fly_palloc(pool, fly_page_convert(sizeof(http_request)));
    request_info *reqinfo = fly_palloc(pool, fly_page_convert(sizeof(request_info)));
    reqinfo->version = fly_palloc(pool, fly_page_convert(sizeof(http_version)));
    req->rinfo = reqinfo;
    return req;
}

int request_operation(int c_sock, fly_pool_t *pool,char *request_line, http_request *req)
{
    /* get request */
    int request_line_length;
	if (strstr(request_line, "\r\n") == NULL)
		return -1;
    request_line_length = strstr(request_line, "\r\n") - request_line;
    req->request_line = (char *) fly_palloc(pool, sizeof(char)*(request_line_length+1));
	if (req->request_line == NULL)
		return -1;
    memcpy(req->request_line, request_line, request_line_length);
    /* get total line */
    req->request_line[request_line_length] = '\0';
    printf("Request Line: %s\n", req->request_line);
    printf("Request Line LENGTH: %ld\n", strlen(req->request_line));
    if (parse_request_line(
		pool,
        c_sock,
        req->request_line,
        req->rinfo
    ) < 0){
        return -1;
    }
    return 0;
}

int header_operation(fly_pool_t *pool, char *headers, http_request *req)
{
    char *header_line = headers;
    char *header_line_end;
    char **header_lines = fly_palloc(pool, fly_page_convert(sizeof(char *)*HEADER_LINES));

    int i = 0;
    while (1){
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

    /* request header too large */
    //if (recv_len == BUF_SIZE){
    //}
    for (int j=0; j<req->header_len; j++){
        printf("HEADER_Line: %s\n", header_lines[j]);
    }
	return 0;
}

void request_free(http_request *req)
{
    free(req->rinfo->uri);
    free(req->request_line);
    free(req->rinfo);
}

void header_free(http_request *req)
{
    for (int i=0;i<req->header_len;i++){
        free(req->header_lines[i]);
    }
    free(req->header_lines);
}

void free_client(http_request *req)
{
    request_free(req);
    header_free(req);
    free(req);
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

	/* register builtin header setting */
	if (fly_hdr_init() == -1){
		perror("fly_hdr_init");
		return -1;
	}
	fly_hdr_t builtin[] = {
		{"Date", fly_date_header, NULL},
		{"Content-Length", fly_content_length_header, NULL},
	};
	for (int i=0; i<(int) (sizeof(builtin)/sizeof(fly_hdr_t));i++)
		fly_register_header(builtin[i].name, builtin[i].trig);

	/* mount point setting */
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
        if (request_operation(conn.c_sock, pool, request_line, req) < 0){
            goto error;
        }
        /* get header */
        char *header_lines = get_header_lines_point(buffer);
        if (header_operation(pool, header_lines, req) == -1){
			fly_500_error(c_sock, pool, req->rinfo->version->full);
			goto error;
		}
        /* get body */
        char *body = get_body_point(buffer);;
        req->body = body;
        printf("BODY: %s\n",body);

        char *res = "HTTP/1.1 200 OK\n\nHello Fly!";
        send(c_sock, res, strlen(res), 0);
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
	fly_hdr_release();
    close(sockfd);
    return 0;
}
