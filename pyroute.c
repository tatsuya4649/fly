#include "pyroute.h"

struct __pyfly_route{
	PyObject_HEAD
	fly_route_reg_t *reg;
//	PyObject *routes;
};
typedef struct __pyfly_route __pyfly_route_t;

static int __pyfly_route_traverse(__pyfly_route_t *self, visitproc visit, void *arg)
{
//	Py_VISIT(self->routes);
	return 0;
}
static int __pyfly_route_clear(__pyfly_route_t *self)
{
//	Py_CLEAR(self->routes);
	return 0;
}

static PyObject *__pyfly_route_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	__pyfly_route_t *self;
	self = (__pyfly_route_t *) type->tp_alloc(type, 0);
	if (self != NULL){
		self->reg = NULL;
//		self->routes = PyList_New(0);
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

//static PyObject *__pyfly_get_routes(__pyfly_route_t *self, void *closure)
//{
//	Py_INCREF(self->routes);
//	return self->routes;
//}
//
//static int __pyfly_set_routes(__pyfly_route_t *self, PyObject *value, void *closure)
//{
//	PyErr_SetString(PyExc_AttributeError, "_routes can't set value.");
//	return -1;
//}

//static PyGetSetDef __pyfly_getsetters[] = {
//	{"_routes", (getter) __pyfly_get_routes, (setter) __pyfly_set_routes, "routes path", NULL},
//	{NULL}
//};

/* Methods */
//static PyObject *__pyfly_routes(__pyfly_route_t *self, PyObject *Py_UNUSED(args))
//{
//	Py_RETURN_NONE;
//}

static PyObject *__pyfly_set_dict(PyObject *uri, PyObject *func, PyObject *method)
{
	PyObject *fly_route_dict;
	fly_route_dict = PyDict_New();

	if (!PyFunction_Check(func)){
		PyErr_SetString(PyExc_TypeError, "route function must be function type.");
		return NULL;
	}
	if (!PyUnicode_Check(uri)){
		PyErr_SetString(PyExc_TypeError, "route uri must be str type.");
		return NULL;
	}
	if (!PyUnicode_Check(method)){
		PyErr_SetString(PyExc_TypeError, "route method must be str type.");
		return NULL;
	}

	if (PyDict_SetItemString(fly_route_dict, "path", uri) == -1){
		PyErr_SetString(PyExc_ValueError, "register dict error.");
		return NULL;
	}
	if (PyDict_SetItemString(fly_route_dict, "func", func)){
		PyErr_SetString(PyExc_ValueError, "register func error.");
		return NULL;
	}
	return fly_route_dict;
}

static PyObject *__pyfly_register_route(__pyfly_route_t *self, PyObject *args)
{
//	PyObject *func;
//	PyObject *fly_route_dict;
//	PyObject *register_path;
//	PyObject *method;
	PyObject *func;
	const char *method;
	const char *uri;
	fly_method_e *m;
	int flag;
	if (!PyArg_ParseTuple(args,"sOs", &uri, &func, &method)){
		goto end;
	}
	if (self->reg == NULL){
		PyErr_SetString(PyExc_ValueError, "register setup error.");
		goto end;
	}
//	fly_route_dict = __pyfly_set_dict(register_path, func, method);
//	if (fly_route_dict == NULL){
//		PyErr_SetString(PyExc_ValueError, "register dict error.");
//		goto end;
//	}
//	if (PyList_Append(self->routes, fly_route_dict) == -1){
//		PyErr_SetString(PyExc_ValueError, "register list error.");
//		goto end;
//	}

	flag = FLY_ROUTE_FLAG_PYTHON;
//	m = fly_match_method_name_e((char *) PyUnicode_AsUTF8(method));
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
//	{"routes", (PyCFunction) __pyfly_routes, METH_NOARGS, "return the routes"},
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
//	.tp_getset = __pyfly_getsetters,
	.tp_traverse = (traverseproc) __pyfly_route_traverse,
/	.tp_clear = (inquiry) __pyfly_route_clear,
};

static PyModuleDef __pyfly_route_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = PYFLY_ROUTE_MODULE_NAME,
	.m_doc = "fly route module for registering uri route path.",
	.m_size = -1,
};

PyMODINIT_FUNC PyInit_fly_route(void)
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
