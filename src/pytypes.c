#include "pyserver.h"

static PyObject *typeobject_new(PyTypeObject *type, PyObject *args, PyObject *kwds)
{
	/*
	 * Can't create instance.
	 *
	 * Use types as only type hints.
	 */
	PyErr_SetString(PyExc_TypeError, "class of types module is used only as hint of type.");
	return NULL;
}

struct pyfly_Request{
	PyObject_HEAD
};
typedef struct pyfly_Request RequestObject;
static PyTypeObject RequestType = {
	.tp_name = "types.Request",
	.tp_doc = "Request object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(RequestObject),
	.tp_new = typeobject_new,
};

struct pyfly_Header{
	PyObject_HEAD
};
typedef struct pyfly_Header HeaderObject;
static PyTypeObject HeaderType = {
	.tp_name = "types.Header",
	.tp_doc = "Header object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(HeaderObject),
	.tp_new = typeobject_new,
};

struct pyfly_Body{
	PyObject_HEAD
};
typedef struct pyfly_Body BodyObject;
static PyTypeObject BodyType = {
	.tp_name = "types.Body",
	.tp_doc = "Body object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(BodyObject),
	.tp_new = typeobject_new,
};

struct pyfly_Cookie{
	PyObject_HEAD
};
typedef struct pyfly_Cookie CookieObject;
static PyTypeObject CookieType = {
	.tp_name = "types.Cookie",
	.tp_doc = "Cookie object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(CookieObject),
	.tp_new = typeobject_new,
};

struct pyfly_Path{
	PyObject_HEAD
};
typedef struct pyfly_Path PathObject;
static PyTypeObject PathType = {
	.tp_name = "types.Path",
	.tp_doc = "Path object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(PathObject),
	.tp_new = typeobject_new,
};

struct pyfly_Query{
	PyObject_HEAD
};
typedef struct pyfly_Query QueryObject;
static PyTypeObject QueryType = {
	.tp_name = "types.Query",
	.tp_doc = "Query object for fly request",
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_itemsize = 0,
	.tp_basicsize = sizeof(QueryObject),
	.tp_new = typeobject_new,
};


static PyModuleDef pyfly_typesmodule = {
	PyModuleDef_HEAD_INIT,
	.m_name = "types",
	.m_doc = "Type module for fly request.",
	.m_size = -1,
};

PyMODINIT_FUNC
PyInit_types(void){
	PyObject *module;
	if (PyType_Ready(&RequestType) < 0)
		return NULL;
	if (PyType_Ready(&HeaderType) < 0)
		return NULL;
	if (PyType_Ready(&BodyType) < 0)
		return NULL;
	if (PyType_Ready(&CookieType) < 0)
		return NULL;
	if (PyType_Ready(&PathType) < 0)
		return NULL;
	if (PyType_Ready(&QueryType) < 0)
		return NULL;

	module = PyModule_Create(&pyfly_typesmodule);
	if (module == NULL)
		return NULL;

	Py_INCREF(&RequestType);
	if (PyModule_AddObject(module, "Request", (PyObject *) &RequestType) < 0)
		goto request_error;
	Py_INCREF(&HeaderType);
	if (PyModule_AddObject(module, "Header", (PyObject *) &HeaderType) < 0)
		goto header_error;
	Py_INCREF(&BodyType);
	if (PyModule_AddObject(module, "Body", (PyObject *) &BodyType) < 0)
		goto body_error;
	Py_INCREF(&CookieType);
	if (PyModule_AddObject(module, "Cookie", (PyObject *) &CookieType) < 0)
		goto cookie_error;
	Py_INCREF(&PathType);
	if (PyModule_AddObject(module, "Path", (PyObject *) &PathType) < 0)
		goto path_error;
	Py_INCREF(&QueryType);
	if (PyModule_AddObject(module, "Query", (PyObject *) &QueryType) < 0)
		goto query_error;

	return module;

query_error:
	Py_DECREF(&QueryType);
path_error:
	Py_DECREF(&PathType);
cookie_error:
	Py_DECREF(&CookieType);
body_error:
	Py_DECREF(&BodyType);
header_error:
	Py_DECREF(&HeaderType);
request_error:
	Py_DECREF(&RequestType);

	Py_DECREF(module);
	return NULL;
}
