#ifndef _FLY_PYTHON_H
#define _FLY_PYTHON_H

#include <Python.h>
#include <structmember.h>
#include "route.h"

struct __pyfly_route{
	PyObject_HEAD
	fly_route_reg_t *reg;
};
typedef struct __pyfly_route __pyfly_route_t;


#define PYFLY_ROUTE_MODULE_NAME			"_fly_route"
#define PYFLY_ROUTE_TYPE_NAME			(PYFLY_ROUTE_MODULE_NAME "._fly_route")

#endif
