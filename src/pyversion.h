#ifndef _PYVERSION_H
#define _PYVERSION_H

#include <Python.h>

typedef struct {
	PyObject_HEAD
	PyFloatObject	*version_float;
	PyObject		*full;
	PyObject		*alpn;
} VersionObject;
#endif
