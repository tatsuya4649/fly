
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
	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "sii", kwlist, &host, &port, &ip_v))
		return -1;
	
	if ((sockfd = fly_socket_init(host, port, ip_v)) == -1)
		return -1;
	self->sockfd = sockfd;
	return 0;
}

static PyTypeObject __pyfly_server_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_SERVER_TYPE_NAME,
	.tp_doc = "",
	.tp_basicsize = sizeof(__pyfly_server_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = __pyfly_server_new,
	.tp_init = (initproc) __pyfly_server_init,
	.tp_dealloc = (destructor) __pyfly_server_dealloc,
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
