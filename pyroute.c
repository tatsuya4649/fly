#include "pyroute.h"

struct __pyfly_route{
	PyObject_HEAD
	fly_route_reg_t *reg;
};
typedef struct __pyfly_route __pyfly_route_t;

static PyTypeObject __pyfly_route_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = "fly_route.FlyRoute",
	.tp_doc = "class for registering route path of uri.",
	.tp_basicsize = sizeof(__pyfly_route_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
};

static PyModuleDef __pyfly_route_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = "fly_route",
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
	if (PyModule_AddObject(m, "FlyRoute", (PyObject *) &__pyfly_route_type) < 0){
		Py_DECREF(&__pyfly_route_type);
		Py_DECREF(m);
		return NULL;
	}

	return m;
}
