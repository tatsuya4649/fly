
enum method_type{
	GET,
	HEAD,
	POST,
	PUT,
	DELETE,
	CONNECT,
	OPTIONS,
	TRACE,
	PATCH,
};

enum version_type{
	V1_1
};

typedef struct{
	char *name;
	enum method_type type;
} http_method;

typedef struct{
	char *full;
	char *number;
	enum version_type type;
} http_version;

http_version versions[] = {
	{"HTTP/1.1", "1.1", V1_1},
	{NULL},
};

http_method methods[] = {
	{"get", GET},
	{"head", HEAD},
	{"post", POST},
	{"put", PUT},
	{"delete", DELETE},
	{"connect", CONNECT},
	{"options", OPTIONS},
	{"trace", TRACE},
	{"patch", PATCH},
	{NULL}
};

typedef struct{
	http_method *method;
	char *uri;
	http_version *version;
} request_info;

typedef struct{
	char *request_line;
	char **header_lines;
	int header_len;
	char *body;
	request_info *rinfo;
} http_request;

