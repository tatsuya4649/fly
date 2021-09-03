
#include "pyserver.h"

struct __pyfly_server{
	PyObject_HEAD
	int sockfd;
};
typedef struct __pyfly_server __pyfly_server_t;

static void __pyfly_server_dealloc(__pyfly_server_t *self)
{
	if (self->sockfd != -1)
		fly_socket_release(self->sockfd);
	fly_fs_release();
	Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyObject *__pyfly_server_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	__pyfly_server_t *self;
	self = (__pyfly_server_t *) type->tp_alloc(type, 0);
	if (self != NULL){
		self->sockfd = -1;
	}
	return (PyObject *) self;
}

static int __pyfly_server_init(__pyfly_server_t *self, PyObject *args, PyObject *kwargs)
{
	char *host;
	int port, ip_v, sockfd;
	static char *kwlist[] = { "host", "port", "ip_v", NULL};
	char error_buf[FLY_ERR_BUFLEN];

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sii", kwlist, &host, &port, &ip_v))
		return -1;
	
	/* Fs Mount */
	if (fly_fs_init() < 0)
		return -1;

	/* Socket */
	sockfd = fly_socket_init(host, port, ip_v);
	switch(sockfd){
	case FLY_EMAKESOCK:
		PyErr_Format(PyExc_ValueError, "can't make socket: (%s:%d)", host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK:
		PyErr_Format(PyExc_ValueError, "can't bind socket: (%s:%d)", host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK_ACCESS:
		PyErr_Format(PyExc_ValueError, "can't bind socket(%s): (%s:%d)", strerror_r(EACCES, error_buf, FLY_ERR_BUFLEN), host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK_ADDRINUSE:
		PyErr_Format(PyExc_ValueError, "can't bind socket(%s): (%s:%d)", strerror_r(EADDRINUSE, error_buf, FLY_ERR_BUFLEN), host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK_BADF:
		PyErr_Format(PyExc_ValueError, "can't bind socket(%s): (%s:%d)", strerror_r(EBADF, error_buf, FLY_ERR_BUFLEN), host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK_INVAL:
		PyErr_Format(PyExc_ValueError, "can't bind socket(%s): (%s:%d)", strerror_r(EINVAL, error_buf, FLY_ERR_BUFLEN), host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EBINDSOCK_NOTSOCK:
		PyErr_Format(PyExc_ValueError, "can't bind socket(%s): (%s:%d)", strerror_r(ENOTSOCK, error_buf, FLY_ERR_BUFLEN), host, port);
		self->sockfd = -1;
		return -1;
	case FLY_ELISTSOCK:
		PyErr_Format(PyExc_ValueError, "can't listen path: (%s:%s)", host, port);
		self->sockfd = -1;
		return -1;
	case FLY_EUNKNOWN_IP:
		PyErr_Format(PyExc_ValueError, "unknown ip version: (%s)", host);
		self->sockfd = -1;
		return -1;
	default:
		break;
	}
	self->sockfd = sockfd;

	return 0;
}

static PyObject *__pyfly_mount_number(__pyfly_server_t *self, PyObject *args)
{
	const char *path;
	long mount_number;
	if (!PyArg_ParseTuple(args, "s", &path))
		return NULL;

	mount_number = (long) fly_mount_number(path);
	switch (mount_number){
	case FLY_SUCCESS:
		return PyLong_FromLong(mount_number);
	case FLY_ENOTFOUND:
		return PyErr_Format(PyExc_ValueError, "not found path: (%s)", path);
	default:
		/* error */
		return PyErr_Format(PyExc_ValueError, "%s", strerror(FLY_ERROR(mount_number)));
	}
}

static PyObject *__pyfly_run(__pyfly_server_t *self, PyObject *args)
{
	__pyfly_route_t *pyroute;
	if (!PyArg_ParseTuple(args, "O", (PyObject *) &pyroute))
		return NULL;
	if (pyroute == NULL)
		return NULL;

	Py_INCREF(pyroute);
    while (1){
        fprintf(stderr, "Waiting Connection...\n");
		/* connection init */
		fly_connect_t *conn;
		conn = fly_connect_init(self->sockfd);
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
		route = fly_found_route(pyroute->reg, req->request_line->uri.uri, req->request_line->method->type);

		if (route == NULL)
			fly_404_error(req->connect->c_sockfd, req->request_line->version->type);
		else{
			/* found route */
			response = NULL;
			if (!(route->flag & FLY_ROUTE_FLAG_PYTHON))
				response = route->function(req);
			fly_response(req->connect->c_sockfd, response, 0);
		}


error:
end_connection:
		fly_request_release(req);
		fly_connect_release(conn);
        continue;
    }
	Py_DECREF(pyroute);
	Py_RETURN_NONE;
}

static PyMethodDef __pyfly_server_methods[] = {
	{"_mount_number", (PyCFunction) __pyfly_mount_number, METH_VARARGS, ""},
	{"run", (PyCFunction) __pyfly_run, METH_VARARGS, ""},
	{NULL}
};

static PyTypeObject __pyfly_server_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_SERVER_TYPE_NAME,
	.tp_doc = "",
	.tp_basicsize = sizeof(__pyfly_server_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = __pyfly_server_new,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc) __pyfly_server_init,
	.tp_dealloc = (destructor) __pyfly_server_dealloc,
	.tp_methods = __pyfly_server_methods,
	.tp_base = &PyBaseObject_Type,
};

static PyModuleDef __pyfly_server_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = PYFLY_SERVER_MODULE_NAME,
	.m_doc = "",
	.m_size = -1,
};

PyMODINIT_FUNC PyInit__fly_server(void)
{
	PyObject *m;
	if (PyType_Ready(&__pyfly_server_type) < 0)
		return NULL;
	
	m = PyModule_Create(&__pyfly_server_module);
	if (m == NULL)
		return NULL;

	Py_INCREF(&__pyfly_server_type);
	if (PyModule_AddObject(m, PYFLY_SERVER_MODULE_NAME, (PyObject *) &__pyfly_server_type) < 0){
		Py_DECREF(&__pyfly_server_type);
		Py_DECREF(m);
		return NULL;
	}
	return m;
}
