#ifdef __cplusplus
extern "C"{
	#include "request.h"
	int parse_request_line(fly_pool_t *pool, __unused int c_sock, fly_reqline_t *req);
}
#endif
#include <gtest/gtest.h>

TEST(REQUEST, fly_get_request_line_ptr)
{
	char *a;
	EXPECT_TRUE(fly_get_request_line_ptr(a) == a);
}

TEST(REQUEST, fly_request_init)
{
	EXPECT_TRUE(fly_request_init() != NULL);
}

TEST(REQUEST, fly_request_release)
{
	fly_request_t *req;
	EXPECT_TRUE((req=fly_request_init()) != NULL);
	EXPECT_TRUE(fly_request_release(req) != -1);
}

fly_reqlinec_t NONSPACE[] = "GET/HTTP/1.1";
fly_reqlinec_t SPACE[] = "    ";
fly_reqlinec_t NONMETHOD[] = "HELLO / HTTP/1.1";
fly_reqlinec_t NONVERSION[] = "GET /";
fly_reqlinec_t NONVERSION2[] = "GET / HTTP";
fly_reqlinec_t UNMVERSION[] = "GET / HTTP/11";
fly_reqlinec_t SUCCESS[]  = "GET / HTTP/1.1";
TEST(REQUEST, parse_request_line)
{
	fly_request_t *req;
	fly_reqline_t *reqline;
	EXPECT_TRUE((reqline = (fly_reqline_t *) fly_pballoc(req->pool, sizeof(fly_reqline_t))) != NULL);

	EXPECT_TRUE((req=fly_request_init()) != NULL);
	req->request_line = reqline;
	/* Success */
	reqline->request_line = SUCCESS;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) != -1);
	/* Failure (req param NULL) */
	EXPECT_TRUE((parse_request_line(req->pool, 0, NULL)) == -1);
	/* Failure (pool param NULL) */
	EXPECT_TRUE((parse_request_line(NULL, 0, req->request_line)) == -1);
	/* Failure (non space) */
	reqline->request_line = NONSPACE;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);
	/* Failure (non match method) */
	reqline->request_line = NONMETHOD;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);
	/* Failure (non version) */
	reqline->request_line = NONVERSION;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);
	/* Failure (non version2) */
	reqline->request_line = NONVERSION2;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);
	/* Failure (unmatch version) */
	reqline->request_line = UNMVERSION;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);
	/* Failure (only space) */
	reqline->request_line = SPACE;
	EXPECT_TRUE((parse_request_line(req->pool, 0, req->request_line)) == -1);


	EXPECT_TRUE(fly_request_release(req) != -1);
}

fly_reqlinec_t REQOP_SUCCESS[] = "GET / HTTP/1.1\r\n";
fly_reqlinec_t NONCRLF[] = "GET / HTTP/1.1";
#define REQOP_SOCK			4
TEST(REQUEST, fly_request_operation)
{
	fly_request_t *req;
	EXPECT_TRUE((req=fly_request_init()) != NULL);

	/* Success */
	EXPECT_TRUE(fly_request_operation(REQOP_SOCK, req->pool, REQOP_SUCCESS, req) != -1);
	/* failure no CRLF */
	EXPECT_TRUE(fly_request_operation(REQOP_SOCK, req->pool, NONCRLF, req) == -1);

	EXPECT_TRUE(fly_request_release(req) != -1);
}

fly_buffer_t HEADERS_SUCCESS[] = "Host: localhost\r\nAccept: text/plain\r\n\r\n";
fly_buffer_t HEADERS_NONCR[] = "Host: localhost\nAccept: text/plain\n\n";
fly_buffer_t HEADERS_NONLF[] = "Host: localhost\rAccept: text/plain\r\r";
fly_buffer_t HEADERS_FIRSTSPACE[] = " Host: localhost\rAccept: text/plain\r\r";
TEST(REQUEST, fly_reqheader_operation)
{
	fly_request_t *req;
	EXPECT_TRUE((req=fly_request_init()) != NULL);

	/* Success */
	EXPECT_TRUE(fly_reqheader_operation(req, HEADERS_SUCCESS) == 0);
	/* Success(no cr) */
	EXPECT_TRUE(fly_reqheader_operation(req, HEADERS_NONCR) == 0);
	/* Failure(no lf) */
	EXPECT_TRUE(fly_reqheader_operation(req, HEADERS_NONLF) == -1);
	/* First Space */
	EXPECT_TRUE(fly_reqheader_operation(req, HEADERS_FIRSTSPACE) == -1);
}
