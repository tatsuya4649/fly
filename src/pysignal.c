
#include "pysignal.h"

struct __pyfly_signal{
	PyObject_HEAD
};
typedef struct __pyfly_signal __pyfly_signal_t;

static int __pyfly_signal_init(__pyfly_signal_t *self, PyObject *args, PyObject *kwargs)
{
	switch (fly_signal_init()){
	case FLY_SUCCESS:
		return 0;
	default:
		return -1;
	}
}

static PyTypeObject __pyfly_signal_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_SIGNAL_TYPE_NAME,
	.tp_doc = "fly signal base module",
	.tp_basicsize = sizeof(__pyfly_signal_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc) __pyfly_signal_init,
	.tp_base = &PyBaseObject_Type,
};

static PyModuleDef __pyfly_signal_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = PYFLY_SIGNAL_MODULE_NAME,
	.m_doc = "for fly signal",
	.m_size = -1,
};

PyMODINIT_FUNC PyInit__fly_signal(void)
{
	PyObject *m;
	if (PyType_Ready(&__pyfly_signal_type) < 0)
		return NULL;
	
	m = PyModule_Create(&__pyfly_signal_module);
	if (m == NULL)
		return NULL;

	Py_INCREF(&__pyfly_signal_type);
	if (PyModule_AddObject(m, "_fly_signal", (PyObject *) &__pyfly_signal_type) < 0){
		Py_DECREF(&__pyfly_signal_type);
		Py_DECREF(m);
	}
	return m;
}
