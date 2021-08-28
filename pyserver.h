#ifndef _PYSERVER_H
#define _PYSERVER_H

#include <Python.h>
#include <structmember.h>
#include "server.h"
#include "fs.h"
#include "err.h"
#include "connect.h"
#include "pyroute.h"

#define PYFLY_SERVER_MODULE_NAME			"_fly_server"
#define PYFLY_SERVER_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "._fly_server")
#endif
