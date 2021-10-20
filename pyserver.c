#include "pyserver.h"

struct __pyfly_server{
	PyObject_HEAD
	fly_master_t *master;
	PyObject *host;
	PyObject *port;
	PyObject *worker;
	PyObject *ssl;
	PyObject *ssl_crt_path;
	PyObject *ssl_key_path;
};

struct PyMemberDef __pyfly_server_members[] = {
	{"_host", T_OBJECT, offsetof(struct __pyfly_server, host), READONLY, ""},
	{"_port", T_OBJECT, offsetof(struct __pyfly_server, port), READONLY, ""},
	{"_worker", T_OBJECT, offsetof(struct __pyfly_server, worker), READONLY, ""},
	{"_ssl", T_OBJECT, offsetof(struct __pyfly_server, ssl), READONLY, ""},
	{"_ssl_crt_path", T_OBJECT, offsetof(struct __pyfly_server, ssl_crt_path), READONLY, ""},
	{"_ssl_key_path", T_OBJECT, offsetof(struct __pyfly_server, ssl_key_path), READONLY, ""},
	{NULL}
};

static PyObject *_host_getter(struct __pyfly_server *self, void *closure);
static int _host_setter(struct __pyfly_server *self, PyObject *value, void *closure);
static PyObject *_port_getter(struct __pyfly_server *self, void *closure);
static int _port_setter(struct __pyfly_server *self, PyObject *value, void *closure);
static PyGetSetDef __pyfly_server_getsetters[] = {
	{"_host", (getter) _host_getter, (setter) _host_setter, "host setting", NULL},
	{"_port", (getter) _port_getter, (setter) _port_setter, "port setting", NULL},
	{NULL},
};

static PyObject *_host_getter(struct __pyfly_server *self, void *closure)
{
	Py_INCREF(self->host);
	return self->host;
}

static int _host_setter(struct __pyfly_server *self, PyObject *value, void *closure)
{
	PyObject *tmp;

	/* attempt delete */
	if (value == NULL){
		PyErr_SetString(PyExc_TypeError, "Cannot delete host attribute.");
		return -1;
	}
	if (!PyUnicode_Check(value)){
		PyErr_SetString(PyExc_TypeError, "host attribute must be str type.");
		return -1;
	}

	tmp = self->host;
	Py_INCREF(value);
	self->host = value;
	Py_DECREF(tmp);
	return 0;
}

static PyObject *_port_getter(struct __pyfly_server *self, void *closure)
{
	Py_INCREF(self->port);
	return self->port;
}

static int _port_setter(struct __pyfly_server *self, PyObject *value, void *closure)
{
	PyObject *tmp;

	/* attempt delete */
	if (value == NULL){
		PyErr_SetString(PyExc_TypeError, "Cannot delete port attribute.");
		return -1;
	}
	if (!PyLong_Check(value)){
		PyErr_SetString(PyExc_TypeError, "port attribute must be int type.");
		return -1;
	}

	tmp = self->port;
	Py_INCREF(value);
	self->port = value;
	Py_DECREF(tmp);
	return 0;
}

typedef struct __pyfly_server __pyfly_server_t;

static void __pyfly_server_dealloc(__pyfly_server_t *self)
{
	if (self->master)
		fly_master_release(self->master);
	Py_TYPE(self)->tp_free((PyObject *) self);
}


static PyObject *__pyfly_server_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	__pyfly_server_t *self;
	self = (__pyfly_server_t *) type->tp_alloc(type, 0);
	return (PyObject *) self;
}

static int pyfly_parse_config_file(void)
{
	return fly_parse_config_file();
}

static fly_master_t *pyfly_master_init(void)
{
	fly_master_t *res;

	res = fly_master_init();
	if (res == NULL)
		PyErr_Format(PyExc_Exception, "parse master init error. (\"%s\")", strerror(errno));

	return res;
}

static int pyfly_create_pidfile_noexit(void)
{
	int res;

	res = fly_create_pidfile_noexit();
	if (res == -1)
		PyErr_Format(PyExc_Exception, "create pidfile error. (\"%s\")", strerror(errno));

	return res;
}

static int pyfly_mount_init(fly_context_t *ctx)
{
	int res;
	res = fly_mount_init(ctx);
	if (res == -1)
		PyErr_Format(PyExc_Exception, "mount init error. (\"%s\")", strerror(errno));

	return res;
}

static int __pyfly_server_init(__pyfly_server_t *self, PyObject *args, PyObject *kwargs)
{
	char *config_path;
	static char *kwlist[] = { "config_path", NULL};
	fly_context_t *ctx;
	fly_master_t *master;

	if (!PyArg_ParseTupleAndKeywords(args, kwargs, "z", kwlist, &config_path))
		return -1;

	if (config_path && setenv(FLY_CONFIG_PATH, config_path, 1) == -1)
		return -1;
	/* config setting */
	if (pyfly_parse_config_file() == -1)
		return -1;

	master = pyfly_master_init();
	if (master == NULL)
		return -1;

	if (pyfly_create_pidfile_noexit() == -1)
		return -1;

	ctx = master->context;
	if (pyfly_mount_init(ctx) < 0)
		return -1;

	self->master = master;
	self->port = PyLong_FromLong((long) fly_server_port());
	self->host = PyUnicode_FromString((const char *) fly_server_host());
	self->worker = PyLong_FromLong((long) master->now_workers);
	self->ssl = PyBool_FromLong((long) fly_ssl());
	self->ssl_crt_path = PyUnicode_FromString((const char *) fly_ssl_crt_path());
	self->ssl_key_path = PyUnicode_FromString((const char *) fly_ssl_key_path());
	return 0;
}

static PyObject *__pyfly_mount_number(__pyfly_server_t *self, PyObject *args)
{
	const char *path;
	long mount_number;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "s", &path))
		return NULL;

	ctx = self->master->context;
	mount_number = (long) fly_mount_number(ctx->mount, path);
	if (mount_number == FLY_ENOTFOUND){
		PyErr_SetString(PyExc_ValueError, "invalid path.");
		return NULL;
	}

	return PyLong_FromLong(mount_number);
}

static int pyfly_mount(fly_context_t *ctx, const char *path)
{
	return fly_mount(ctx, path);
}

static PyObject *__pyfly_mount(__pyfly_server_t *self, PyObject *args)
{
	const char *__path;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "s", &__path))
		return NULL;

	ctx = self->master->context;
	if (pyfly_mount(ctx, __path) == -1)
		return NULL;

	Py_RETURN_NONE;
}

static PyObject *__pyfly_run(__pyfly_server_t *self, PyObject *args)
{
	bool daemon;

	if (!PyArg_ParseTuple(args, "p", &daemon))
		return NULL;

	if (daemon){
		fprintf(stderr, "To be daemon process...");
		if (fly_master_daemon() == -1){
			PyErr_SetString(PyExc_RuntimeError, "to be daemon process error.");
			return NULL;
		}
	}

	fly_master_worker_spawn(self->master, fly_worker_process);
	fly_master_process(self->master);

	Py_RETURN_NONE;
}

static PyObject *__pyfly_mount_files(__pyfly_server_t *self, PyObject *args)
{
	int __mn, mount_files_count;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "i", &__mn))
		return NULL;

	ctx = self->master->context;

	mount_files_count = fly_mount_files_count(ctx->mount, __mn);
	if (mount_files_count == -1){
		PyErr_SetString(PyExc_ValueError, "invalid mount number.");
		return NULL;
	}
	return PyLong_FromLong((long) mount_files_count);
}

static PyMethodDef __pyfly_server_methods[] = {
	{"_mount_number", (PyCFunction) __pyfly_mount_number, METH_VARARGS, ""},
	{"_mount", (PyCFunction) __pyfly_mount, METH_VARARGS, ""},
	{"run", (PyCFunction) __pyfly_run, METH_VARARGS, ""},
	{"_mount_files", (PyCFunction) __pyfly_mount_files, METH_VARARGS, ""},
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
	.tp_members = __pyfly_server_members,
	.tp_getset = __pyfly_server_getsetters,
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
