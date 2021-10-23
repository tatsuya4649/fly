#ifndef _PYSERVER_H
#define _PYSERVER_H

#include <Python.h>
#include <structmember.h>
#include "server.h"
#include "context.h"
#include "mount.h"
#include "err.h"
#include "connect.h"
#include "master.h"

#define PYFLY_SERVER_MODULE_NAME			"_fly_server"
#define PYFLY_SERVER_NAME					"_fly_server"
#define PYFLY_VERSION_NAME					"_fly_version"
#define PYFLY_SCHEME_NAME					"_fly_scheme"
#define PYFLY_SERVER_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_SERVER_NAME)
#define PYFLY_VERSION_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_VERSION_NAME)
#define PYFLY_SCHEME_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_SCHEME_NAME)
#endif
