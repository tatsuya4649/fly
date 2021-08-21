#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "response.h"
#include "header.h"
#include "alloc.h"
#include "fs.h"
#include "math.h"

response_code responses[] = {
	/* 1xx Info */
	/* 2xx Success */
	{200, _200, "Success"},
	/* 3xx Redirect */
	/* 4xx Client Error */
	{400, _400, "Bad Request"},
	{401, _401, "Unauthorized"},
	{402, _402, "Payment Required"},
	{403, _403, "Forbidden"},
	{404, _404, "Not Found"},
	{405, _405, "Method Not Allowed"},
	{406, _406, "Not Acceptable"},
	{407, _407, "Proxy Authentication Required"},
	{408, _408, "Request Timeout"},
	{409, _409, "Conflict"},
	{409, _409, "Gone"},
	{410, _410, "Length Required"},
	{411, _411, "Precondition Failed"},
	{412, _412, "Precondition Failed"},
	{413, _413, "Request Entiry Too Large"},
	{414, _414, "Request-URI Too Long"},
	{415, _415, "Unsupported Media Type"},
	{416, _416, "Requested Range Not Satisfiable"},
	{417, _417, "Expectation Failed"},
	/* 5xx Server Error */
	{500, _500, "Server Error"},
	{-1, -1, NULL}
};

char *alloc_response_content(fly_pool_t *pool,char *now,int *before,int incre)
{
	char *resp;
	resp = fly_palloc(pool, fly_page_convert(*before+incre));
	if (resp != now)
		memcpy(resp, now, *before);
	*before += incre;
	return resp;
}

char *update_alloc_response_content(
	fly_pool_t *pool,
	char *response_content,
	int pos,
	int length,
	int total_length,
	int incre
)
{
	while ((pos+length) > total_length){
		response_content = alloc_response_content(
			pool,
			response_content,
			&total_length,
			incre
		);
	}
	return response_content;
}

char *add_cr_lf(
	fly_pool_t *pool,
	char *response_content,
	int *pos,
	int total_length,
	int incre
)
{
	char *res;
	res = update_alloc_response_content(
		pool,
		response_content,
		*pos,
		CRLF_LENGTH,
		total_length,
		incre
	);
	strcpy(&response_content[*pos], "\r\n");
	*pos += CRLF_LENGTH;
	return res;
}

static char *response_raw(http_response *res, fly_pool_t *pool, int *send_len)
{
	char *response_content = NULL;
	int total_length = 0;
	int pos = 0;

	response_content = alloc_response_content(
		pool,
		response_content,
		&total_length,
		RESPONSE_LENGTH_PER
	);
	/* status_line */
	strcpy(&response_content[pos],res->status_line);
	pos += (int) strlen(res->status_line);
	add_cr_lf(
		pool,
		response_content,
		&pos,
		total_length,
		RESPONSE_LENGTH_PER
	);
	/* header */
	if (res->header!=NULL && res->header_len>0){
		update_alloc_response_content(
			pool,
			response_content,
			pos,
			(int) res->header_len,
			total_length,
			RESPONSE_LENGTH_PER
		);
		memcpy(&response_content[pos], res->header, res->header_len);
		pos += res->header_len;
	}
	if (res->header_len != 0 && res->body_len > 0){
		add_cr_lf(pool, response_content, &pos, total_length, RESPONSE_LENGTH_PER);
	}
	/* body */
	if (res->body!=NULL && res->body_len>0){
		update_alloc_response_content(
			pool,
			response_content,
			pos,
			(int) res->body_len,
			total_length,
			RESPONSE_LENGTH_PER
		);
		memcpy(&response_content[pos], res->body, res->body_len);
		pos += res->body_len;
	}
	*send_len = pos;
	return response_content;
}

char *fly_code_explain(fly_rescode_t type)
{
	for (response_code *res=responses; res->status_code!=-1; res++){
		if (res->type == type)
			return res->explain;
	}
	return NULL;
}

int response_code_from_type(enum response_code_type type)
{
	for (response_code *res=responses; res->status_code!=-1; res++){
		if (res->type == type)
			return res->status_code;
	}
	return -1;
}

int fly_send(int c_sockfd, char *buffer, int send_len, int flag)
{
	int send_result;
	while(1){
		send_result = send(c_sockfd, buffer, send_len, flag);
		if (send_result == -1)
			return -1;
		if (send_result == send_len){
			break;
		}
		buffer += send_result;
		send_len -= send_result;
	}
	return 0;
}

void __fly_400_error(int c_sockfd, fly_version_e version)
{
	fly_pool_t *respool;
	fly_hdr_ci *ci;
	char *contlen_str;
	char *header;
	int contlen = strlen(fly_code_explain(_400));

	ci = fly_header_init();
	respool = fly_response_init();

	contlen_str = fly_pballoc(ci->pool, fly_number_digits(contlen)+1);
	sprintf(contlen_str, "%d", contlen);
	if (fly_header_add(ci, "Content-Length", contlen_str) == -1)
		goto error;
	if (fly_header_add(ci, "Connection", "close") == -1)
		goto error;

	header = fly_header_from_chain(ci);
    if (fly_response(
		c_sockfd,
		respool,
		_400,
		version,
		header,
		strlen(header),
		fly_code_explain(_400),
		strlen(fly_code_explain(_400)),
		0
	) == -1)
		goto error;
	
	fly_header_release(ci);
	fly_response_release(respool);
	return;

error:
	fly_500_error(c_sockfd, V1_1);

	fly_header_release(ci);
	fly_response_release(respool);
	return;
}

#define __alias_fly_400_error		\
	__attribute__ ((weak, alias("fly_400_error")))
__attribute__ ((weak, alias("__fly_400_error"))) void fly_400_error(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_notfound_request_line(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_notfound_request_method(int c_sockfd, fly_version_e version);
__alias_fly_400_error  void fly_unmatch_request_method(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_notfound_uri(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_notfound_http_version(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_unmatch_http_version(int c_sockfd, fly_version_e version);
__alias_fly_400_error void fly_nonumber_http_version(int c_sockfd, fly_version_e version);

void fly_500_error(int c_sockfd, fly_version_e version)
{
	fly_pool_t *respool;
	char *status_line;
	char verstr[FLY_VERSION_MAXLEN];

	respool = fly_response_init();
	status_line = fly_palloc(respool, fly_page_convert(FLY_STATUS_LINE_MAX));
	if (fly_version_str(verstr, version) == -1)
		goto error;

	sprintf(
		status_line,
		"%s %d %s",
		verstr,
		response_code_from_type(_500),
		fly_code_explain(_500)
	);
	fly_send(c_sockfd, status_line, strlen(status_line), 0);
error:
	fly_response_release(respool);
	return;
}

fly_pool_t *fly_response_init(void)
{
	return fly_create_pool(FLY_RESPONSE_POOL_PAGE);
}

int fly_response(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	fly_version_e version,
	char *header,
	int header_len,
	char *body,
	ssize_t body_len,
	__unused fly_flag_t flag
){
	http_response res_content;
	__unused int send_result, send_len;
	char *send_start;
	char verstr[FLY_VERSION_MAXLEN];
	int rescode;
	char *resexp;

	res_content.status_line = fly_palloc(respool, fly_page_convert(sizeof(char)*FLY_STATUS_LINE_MAX));
	if (res_content.status_line == NULL)
		goto error;
	if (fly_version_str(verstr,version) == -1)
		goto error;
	if ((rescode = response_code_from_type(response_code)) == -1)
		goto error;
	if ((resexp = fly_code_explain(response_code)) == NULL)
		goto error;
	sprintf(res_content.status_line,"%s %d %s", verstr, rescode, resexp);
	res_content.header = header;

	if (header != NULL && res_content.header == NULL){
		fly_500_error(c_sockfd,  version);
		goto error;
	}

	res_content.header_len = header_len;
	res_content.body = body;
	res_content.body_len = body_len;

	send_start = response_raw(&res_content, respool, &send_len);
	send_result = fly_send(c_sockfd, send_start, send_len, 0);

	return 0;
error:
	return -1;
}

int fly_response_file(
	int c_sockfd,
	fly_pool_t *respool,
	int response_code,
	fly_version_e version,
	char *header_lines,
	int header_len,
	char *file_path,
	int mount_number,
	fly_pool_s size,
	fly_flag_t flag
){
	char *body = fly_from_path(respool, size, mount_number, file_path);
	ssize_t body_len = fly_file_size(file_path);
	if (body_len == -1)
		return -1;
	return fly_response(c_sockfd, respool, response_code, version, header_lines, header_len, body, body_len, flag);
}

int fly_response_release(fly_pool_t *respool)
{
	if (respool != NULL)
		return fly_delete_pool(respool);
	return 0;
}

