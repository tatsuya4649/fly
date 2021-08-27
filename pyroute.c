#include "pyroute.h"

struct __pyfly_route{
	PyObject_HEAD
	fly_route_reg_t *reg;
};
typedef struct __pyfly_route __pyfly_route_t;

static int __pyfly_route_traverse(__pyfly_route_t *self, visitproc visit, void *arg)
{
	return 0;
}
static int __pyfly_route_clear(__pyfly_route_t *self)
{
	return 0;
}

static PyObject *__pyfly_route_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	__pyfly_route_t *self;
	self = (__pyfly_route_t *) type->tp_alloc(type, 0);
	if (self != NULL){
		self->reg = NULL;
	}
	if (fly_route_init() == -1)
		return NULL;
	return (PyObject *) self;
}

static int __pyfly_route_init(__pyfly_route_t *self, PyObject *args, PyObject *kwds)
{
	self->reg = fly_route_reg_init();
	if (self->reg == NULL)
		return -1;
	
	return 0;
}

static PyMemberDef __pyfly_route_members[] = {
	{NULL}
};

static PyObject *__pyfly_register_route(__pyfly_route_t *self, PyObject *args)
{
	const char *method;
	const char *uri;
	fly_method_e *m;
	int flag;
	if (!PyArg_ParseTuple(args,"ss", &uri, &method)){
		goto end;
	}
	if (self->reg == NULL){
		PyErr_SetString(PyExc_ValueError, "register setup error.");
		goto end;
	}

	flag = FLY_ROUTE_FLAG_PYTHON;
	m = fly_match_method_name_e((char *) method);
	if (m == NULL){
		PyErr_SetString(PyExc_ValueError, "fly method unmatch error");
		goto end;
	}
	if (fly_register_route(self->reg, NULL, uri, *m, flag) == -1){
		PyErr_SetString(PyExc_ValueError, "fly route register error");
		goto end;
	}
end:
	Py_RETURN_NONE;
}

static PyMethodDef __pyfly_methods[] = {
	{"register_route", (PyCFunction) __pyfly_register_route, METH_VARARGS, "register route passed uri path"},
	{NULL}
};

static PyTypeObject __pyfly_route_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_ROUTE_TYPE_NAME,
	.tp_doc = "class for registering route path of uri.",
	.tp_basicsize = sizeof(__pyfly_route_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE | Py_TPFLAGS_HAVE_GC,
	.tp_new = __pyfly_route_new,
	.tp_init = (initproc) __pyfly_route_init,
	.tp_members = __pyfly_route_members,
	.tp_methods = __pyfly_methods,
	.tp_traverse = (traverseproc) __pyfly_route_traverse,
	.tp_clear = (inquiry) __pyfly_route_clear,
};

static PyModuleDef __pyfly_route_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = PYFLY_ROUTE_MODULE_NAME,
	.m_doc = "fly route module for registering uri route path.",
	.m_size = -1,
};

PyMODINIT_FUNC PyInit__fly_route(void)
{
	PyObject *m;
	if (PyType_Ready(&__pyfly_route_type) < 0)
		return NULL;
	
	m = PyModule_Create(&__pyfly_route_module);
	if (m == NULL)
		return NULL;
	
	Py_INCREF(&__pyfly_route_type);
	if (PyModule_AddObject(m, "_fly_route", (PyObject *) &__pyfly_route_type) < 0){
		Py_DECREF(&__pyfly_route_type);
		Py_DECREF(m);
		return NULL;
	}

	return m;
}
