#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include "response.h"
#include "header.h"
#include "alloc.h"

response_code responses[] = {
	{400, _400, ""},
	{401, _401, ""},
	{402, _402, ""},
	{403, _403, ""},
	{404, _404, ""},
	{405, _405, ""},
	{406, _406, ""},
	{407, _407, ""},
	{408, _408, ""},
	{409, _409, ""},
	{409, _409, ""},
	{410, _410, ""},
	{411, _411, ""},
	{412, _412, ""},
	{413, _413, ""},
	{414, _414, ""},
	{415, _415, ""},
	{416, _416, ""},
	{417, _417, ""},
	{500, _500, "Server Error"},
	{-1, -1, NULL}
};

char *alloc_response_content(char *now,int *before,int incre)
{
	char *resp;
	resp = malloc(*before+incre);
	if (resp != now)
		memcpy(resp, now, *before);
	*before += incre;
	return resp;
}

char *update_alloc_response_content(
	char *response_content,
	int pos,
	int length,
	int total_length,
	int incre
)
{
	while ((pos+length) > total_length){
		response_content = alloc_response_content(
			response_content,
			&total_length,
			incre
		);
	}
	return response_content;
}

char *add_cr_lf(
	char *response_content,
	int *pos,
	int total_length,
	int incre
)
{
	char *res;
	res = update_alloc_response_content(
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

char *response_raw(http_response *res, int *send_len)
{
	char *response_content = NULL;
	int total_length = 0;
	int pos = 0;

	response_content = alloc_response_content(
		response_content,
		&total_length,
		RESPONSE_LENGTH_PER
	);
	/* status_line */
	strcpy(&response_content[pos],res->status_line);
	pos += (int) strlen(res->status_line);
	add_cr_lf(
		response_content,
		&pos,
		total_length,
		RESPONSE_LENGTH_PER
	);
	/* header */
	for (int i=0;i<res->header_len;i++){
		update_alloc_response_content(
			response_content,
			pos,
			(int) strlen(res->header_lines[i]),
			total_length,
			RESPONSE_LENGTH_PER
		);
		strcpy(&response_content[pos],res->header_lines[i]);
		pos += (int) strlen(res->header_lines[i]);
		add_cr_lf(
			response_content,
			&pos,
			total_length,
			RESPONSE_LENGTH_PER
		);
	}
	if (res->header_len != 0){
		add_cr_lf(
			response_content,
			&pos,
			total_length,
			RESPONSE_LENGTH_PER
		);
	}
	/* body */
	if (res->body!=NULL && res->body_len>0){
		update_alloc_response_content(
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

void response_free(char *res)
{
	free(res);
}

char *get_default_version(void)
{
	return DEFAULT_RESPONSE_VERSION;
}

char *get_version(char *version)
{
	if (version == NULL){
		return get_default_version();
	}
	return version;
}

char *get_code_explain(enum response_code_type type)
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

void response_500_error(int c_sockfd, fly_pool_t *pool, char *version)
{
	char *status_line = fly_palloc(pool, fly_page_convert(FLY_STATUS_LINE_MAX));
	sprintf(
		status_line,
		"HTTP/%s %d %s",
		version ? get_version(version) : DEFAULT_RESPONSE_VERSION,
		response_code_from_type(_500),
		get_code_explain(_500)
	);
	fly_send(c_sockfd, status_line, strlen(status_line), 0);
}

int fly_response(
	int c_sockfd,
	int response_code,
	char *version,
	fly_hdr_t *header_lines,
	int header_len,
	char *body,
	int body_len
){
	fly_pool_t *respool;
	respool = fly_create_pool(FLY_RESPONSE_POOL_PAGE);
	http_response res_content;
	__attribute__((unused)) int send_result, send_len;
	char *send_start;
	res_content.status_line = fly_palloc(respool, fly_page_convert(sizeof(char)*FLY_STATUS_LINE_MAX));
	if (res_content.status_line == NULL)
		goto error;
	sprintf(res_content.status_line,"HTTP/%s %d %s", get_version(version), response_code_from_type(response_code), get_code_explain(response_code));
	res_content.header_lines = fly_hdr_eles_to_string(header_lines, respool, &header_len, body, body_len);

	if (res_content.header_lines == NULL){
		response_500_error(c_sockfd, respool, version);
		goto error;
	}

	res_content.header_len = header_len;
	res_content.body = body;
	res_content.body_len = body_len;

	send_start = response_raw(&res_content, &send_len);
	send_result = fly_send(c_sockfd, send_start, send_len, 0);

	fly_delete_pool(respool);
	return 0;
error:
	fly_delete_pool(respool);
	return -1;
}


