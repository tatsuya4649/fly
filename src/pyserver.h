#ifndef _PYSERVER_H
#define _PYSERVER_H

#include <Python.h>
#include <structmember.h>
#include "../config.h"
#include "server.h"
#include "context.h"
#include "char.h"
#include "mount.h"
#include "err.h"
#include "connect.h"
#include "master.h"
#include "header.h"
#include "body.h"
#include "response.h"
#include "conf.h"

#define PYFLY_SERVER_MODULE_NAME			"_fly_server"
#define PYFLY_SERVER_NAME					"_fly_server"
#define PYFLY_VERSION_NAME					"_fly_version"
#define PYFLY_SCHEME_NAME					"_fly_scheme"
#define PYFLY_RESPONSE_MODULE_NAME			"_fly_response"
#define PYFLY_RESPONSE_NAME					"_fly_response"
#define PYFLY_SERVER_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_SERVER_NAME)
#define PYFLY_VERSION_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_VERSION_NAME)
#define PYFLY_SCHEME_TYPE_NAME			(PYFLY_SERVER_MODULE_NAME "." PYFLY_SCHEME_NAME)
#define PYFLY_RESPONSE_TYPE_NAME			(PYFLY_RESPONSE_MODULE_NAME "." PYFLY_RESPONSE_NAME)
#endif

