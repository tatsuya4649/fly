#include "pyserver.h"
#include "v2.h"
#ifdef DEBUG
#include "log.h"
#endif
#include "err.h"

struct __pyfly_server{
	PyObject_HEAD
	fly_master_t		*master;
	const char			*host;
	long				port;
	long 				worker;
	long 				reqworker;
	bool 				ssl;
	char				*log;
	char				*ssl_crt_path;
	char 				*ssl_key_path;
};

struct PyMemberDef __pyfly_server_members[] = {
	{"_host", T_STRING, offsetof(struct __pyfly_server, host), READONLY, ""},
	{"_port", T_LONG, offsetof(struct __pyfly_server, port), READONLY, ""},
	{"_worker", T_LONG, offsetof(struct __pyfly_server, worker), READONLY, ""},
	{"_log", T_STRING, offsetof(struct __pyfly_server, log), READONLY, ""},
	{"_reqworker", T_LONG, offsetof(struct __pyfly_server, reqworker), READONLY, ""},
	{"_ssl", T_BOOL, offsetof(struct __pyfly_server, ssl), READONLY, ""},
	{"_ssl_crt_path", T_STRING, offsetof(struct __pyfly_server, ssl_crt_path), READONLY, ""},
	{"_ssl_key_path", T_STRING, offsetof(struct __pyfly_server, ssl_key_path), READONLY, ""},
	{NULL}
};

typedef struct __pyfly_version{
	PyObject_HEAD
	float			version_float;
	const char		*full;
	const char		*alpn;
} VersionObject;

static int VersionObject_init(VersionObject *self, PyObject *args)
{
	if (PyArg_ParseTuple(args, "fss", &self->version_float, &self->full, &self->alpn) == -1)
		return -1;
	return 0;
}

struct PyMemberDef VersionObject_member[] = {
	{"version", T_FLOAT, offsetof(VersionObject, version_float), READONLY},
	{"full",	T_STRING, offsetof(VersionObject, full), READONLY},
	{"alpn",	T_STRING, offsetof(VersionObject, alpn), READONLY},
	{NULL}
};

struct PyMethodDef VersionObject_method[] = {
	{NULL}
};

static void VersionObject_dealloc(VersionObject *self)
{
	Py_TYPE(self)->tp_free((PyObject *) self);
}

static PyTypeObject VersionObjectType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_VERSION_TYPE_NAME,
	.tp_doc = "",
	.tp_basicsize = sizeof(VersionObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor) VersionObject_dealloc,
	.tp_init = (initproc) VersionObject_init,
	.tp_members = VersionObject_member,
	.tp_methods = VersionObject_method,
	.tp_base = &PyBaseObject_Type,
};

typedef struct __pyfly_scheme{
	PyObject_HEAD
	const char *name;
} SchemeObject;

static int SchemeObject_init(SchemeObject *self, PyObject *args)
{
	if (PyArg_ParseTuple(args, "s", &self->name) == -1)
		return -1;
	return 0;
}

static void SchemeObject_dealloc(VersionObject *self)
{
	Py_TYPE(self)->tp_free((PyObject *) self);
}


struct PyMemberDef SchemeObject_member[] = {
	{"name", T_STRING, offsetof(SchemeObject, name), READONLY},
	{NULL}
};

struct PyMethodDef SchemeObject_method[] = {
	{NULL}
};

static PyTypeObject SchemeObjectType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_SCHEME_TYPE_NAME,
	.tp_doc = "",
	.tp_basicsize = sizeof(SchemeObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT,
	.tp_new = PyType_GenericNew,
	.tp_dealloc = (destructor) SchemeObject_dealloc,
	.tp_init = (initproc) SchemeObject_init,
	.tp_members = SchemeObject_member,
	.tp_methods = SchemeObject_method,
	.tp_base = &PyBaseObject_Type,
};

typedef struct {
	PyObject_HEAD
} FlyResponseObject;

static void FlyResponse_dealloc(FlyResponseObject *self)
{
	Py_TYPE(self)->tp_free((PyObject *) self);
}

static int FlyResponse_init(FlyResponseObject *self, PyObject *args, PyObject *kwds)
{
	return 0;
}

PyTypeObject FlyResponseType = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_RESPONSE_TYPE_NAME,
	.tp_doc = "_fly_response._fly_response",
	.tp_basicsize = sizeof(FlyResponseObject),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT|Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc) FlyResponse_init,
	.tp_dealloc = (destructor) FlyResponse_dealloc,
};


static PyGetSetDef __pyfly_server_getsetters[] = {
	{NULL},
};

typedef struct __pyfly_server __pyfly_server_t;

static void __pyfly_server_dealloc(__pyfly_server_t *self)
{
//	if (self->master)
//		fly_master_release(self->master);
	Py_TYPE(self)->tp_free((PyObject *) self);
}


__fly_unused static PyObject *__pyfly_server_new(PyTypeObject *type, PyObject *args, PyObject *kwargs)
{
	__pyfly_server_t *self;
	self = (__pyfly_server_t *) type->tp_alloc(type, 0);
	return (PyObject *) self;
}

static int pyfly_parse_config_file(void)
{
	int res;

	res = fly_parse_config_file();
	switch (res){
	case FLY_PARSE_CONFIG_NOTFOUND:
		break;
	case FLY_PARSE_CONFIG_ERROR:
		PyErr_SetString(PyExc_ValueError, "parse file error.");
		break;
	case FLY_PARSE_CONFIG_SUCCESS:
		break;
	case FLY_PARSE_CONFIG_SYNTAX_ERROR:
		PyErr_SetString(PyExc_ValueError, "parse file error.");
		break;
	default:
		PyErr_SetString(PyExc_ValueError, "parse file error.");
		break;
	}
	return res;
}

static fly_master_t *pyfly_master_init(void)
{
	struct fly_err err;
	fly_master_t *res;

	res = fly_master_init(&err);
	if (res == NULL)
		PyErr_Format(PyExc_Exception,
			"Master init. %s", err.content);

	return res;
}

static int pyfly_create_pidfile_noexit(void)
{
	int res;

	res = fly_create_pidfile_noexit();
	if (res == -1)
		PyErr_Format(PyExc_Exception, "create pidfile error. (\"%s\")", strerror(errno));

	return res;
}

static int pyfly_mount_init(fly_context_t *ctx)
{
	int res;
	res = fly_mount_init(ctx);
	if (res == -1)
		PyErr_Format(PyExc_Exception, "mount init error. (\"%s\")", strerror(errno));

	return res;
}

static int __pyfly_server_init(__pyfly_server_t *self, PyObject *args, PyObject *kwargs)
{
	self->master = NULL;
	self->port = 0;
	self->host = NULL;
	self->worker = 0;
	self->reqworker = 0;
	self->ssl = false;
	self->ssl_crt_path = NULL;
	self->ssl_key_path = NULL;
	self->log = NULL;
	return 0;
}

/*
 *	C <---> Python communication function in response.
 *
 *	required Python dictionary keys (response).
 *	WARNING: content_length is auto setting.
 *	- body(byte)
 *	- header(list)
 *	- status_code(int)
 */
extern PyTypeObject FlyResponseType;
#define PYFLY_BODY_KEY							"body"
#define PYFLY_HEADER_KEY						"header"
#define PYFLY_STATUS_CODE_KEY					"status_code"
#define PYFLY_CONTENT_TYPE_KEY					"content_type"
static fly_response_t *pyfly_route_handler(fly_request_t *request, fly_route_t *route, void *data)
{
#define PYFLY_RESHANDLER_ARGS_COUNT			1
	PyObject *__func, *__args, *__reqdict, *pyres=NULL, *__margs;
	fly_response_t *res=NULL;
	fly_reqline_t *reqline;
	fly_uri_t *uri;
	struct fly_http_version *ver;
	fly_query_t *query;
	fly_hdr_ci *header;
	fly_body_t *body;
	fly_method_e request_method;
	fly_scheme_t *scheme;
	struct fly_bllist *__b;

	__func = (PyObject *) data;
	Py_INCREF(__func);
	__reqdict = PyDict_New();
	/* C request --> Python request */
	reqline = request->request_line;
	request_method = reqline->method->type;
	uri = &reqline->uri;
	PyObject *__pyuri = PyUnicode_FromStringAndSize((const char *) uri->ptr, (Py_ssize_t) uri->len);
	if (PyDict_SetItemString(__reqdict, "uri", __pyuri) == -1)
		return NULL;

	Py_DECREF(__pyuri);

	query = &reqline->query;

	PyObject *__pyquery;

	if (query->ptr != NULL)
		__pyquery = PyUnicode_FromStringAndSize((const char *) query->ptr, (Py_ssize_t) query->len);
	else{
		Py_INCREF(Py_None);
		__pyquery = Py_None;
	}

	if (PyDict_SetItemString(__reqdict, "query", __pyquery) == -1)
		return NULL;
	Py_DECREF(__pyquery);

	/* HTTP version */
	ver = reqline->version;
	__margs = Py_BuildValue("fss", atof(ver->number), ver->full, ver->alpn);
	VersionObject *__pyver = (VersionObject *) PyObject_CallObject((PyObject *) &VersionObjectType, __margs);
	if (PyDict_SetItemString(__reqdict, "version", (PyObject *) __pyver) == -1)
		return NULL;

	Py_DECREF(__pyver);
	Py_DECREF(__margs);
	/* HTTP Scheme */
	scheme = reqline->scheme;
	__margs = Py_BuildValue("(s)", scheme->name);
	SchemeObject *__pyscheme = (SchemeObject *) PyObject_CallObject((PyObject *) &SchemeObjectType, __margs);
	if (PyDict_SetItemString(__reqdict, "scheme", (PyObject *) __pyscheme) == -1)
		return NULL;

	Py_DECREF(__margs);
	Py_DECREF(__pyscheme);
	/* Peer connection info */
	PyObject *__pyhost = PyUnicode_FromString((const char *) request->connect->hostname);
	if (PyDict_SetItemString(__reqdict, "host", (PyObject *) __pyhost) == -1)
		return NULL;

	PyLongObject *__pyport = (PyLongObject *) PyLong_FromString((const char *) request->connect->servname, NULL, 10);
	if (PyDict_SetItemString(__reqdict, "port", (PyObject *) __pyport) == -1)
		return NULL;

	Py_DECREF(__pyhost);
	Py_DECREF(__pyport);

	/* Path parameter */
	if (fly_path_param_count(&reqline->path_params) > 0){
		struct fly_param *__pp;
		int i=0;
		PyListObject *__pyparams = (PyListObject *) PyList_New((Py_ssize_t) fly_path_param_count(&reqline->path_params));
		fly_for_each_bllist(__b, &reqline->path_params.params){
			__pp = fly_bllist_data(__b, struct fly_param, blelem);

			PyDictObject *__pd = (PyDictObject *) PyDict_New();
			PyObject *__n, *type, *value;

			__n = PyUnicode_FromStringAndSize(__pp->var_ptr, __pp->var_len);
			if (__n == NULL || PyDict_SetItemString((PyObject *) __pd, "param", __n) == -1)
				return NULL;

			type = PyLong_FromLong((long) __pp->type);
			if (type == NULL || PyDict_SetItemString((PyObject *) __pd, "type", type) == -1)
				return NULL;

			value = PyUnicode_FromStringAndSize((const char *) __pp->value, (Py_ssize_t) __pp->value_len);
			if (value == NULL || PyDict_SetItemString((PyObject *) __pd, "value", value) == -1)
				return NULL;


			if (PyList_SetItem((PyObject *) __pyparams, i, (PyObject *) __pd) == -1)
				return NULL;
			Py_DECREF(__n);
			Py_DECREF(type);
			Py_DECREF(value);
			i++;
		}
		if (PyDict_SetItemString((PyObject *) __reqdict, "path_params", (PyObject *) __pyparams) == -1)
			return NULL;
		Py_DECREF(__pyparams);
	}
	/* Header */
	if (request->header){
		header = request->header;
		PyListObject *__pylist = (PyListObject *) PyList_New((Py_ssize_t) header->chain_count);
		PyListObject *__pycookie = (PyListObject *) PyList_New(0);
		fly_hdr_c *__c;
		int i=0;
		fly_for_each_bllist(__b, &header->chain){
			__c = fly_bllist_data(__b, fly_hdr_c, blelem);
			PyDictObject *__hd = (PyDictObject *) PyDict_New();
			PyObject *__n, *__v;

			__n = PyUnicode_FromStringAndSize(__c->name, __c->name_len);
			if (PyDict_SetItemString((PyObject *) __hd, "name", __n) == -1)
				return NULL;
			__v = PyUnicode_FromStringAndSize(__c->value, __c->value_len);
			if (PyDict_SetItemString((PyObject *) __hd, "value", __v) == -1)
				return NULL;

			if (PyList_SetItem((PyObject *) __pylist, i, (PyObject *) __hd) == -1)
				return NULL;

			if (__c->cookie){
				PyObject *__cookie;

				__cookie = PyUnicode_FromStringAndSize(__c->value, __c->value_len);
//				PyDictObject *__cookie = (PyDictObject *) PyDict_New();
//				PyObject *__cn=NULL, *__cv=NULL;
//				char *__cn_ptr=NULL, *__cv_ptr=NULL, *np;
//				size_t __cn_len=0, __cv_len=0;
//
//				/* FIXME: INVALID PARSE */
//				np = __c->value;
//				__cn_ptr = __c->value;
//				while(!fly_equal(*np) && \
//						np<__c->value+__c->value_len-1){
//					np++;
//				}
//
//				__cn_len = np-__cn_ptr;
//				if (++np<=__c->value+__c->value_len-1){
//					__cv_ptr = np;
//					__cv_len = (size_t) ((__c->value+__c->value_len)-np);
//#ifdef DEBUG
//					assert(__cv_len>0);
//#endif
//				}
//
//				__cn = PyUnicode_FromStringAndSize(__cn_ptr, __cn_len);
//				if (PyDict_SetItemString((PyObject *) __cookie, "name", __cn) == -1)
//					return NULL;
//
//				if (__cv_ptr){
//					__cv = PyUnicode_FromStringAndSize(__cv_ptr, __cv_len);
//					if (PyDict_SetItemString((PyObject *) __cookie, "value", __cv) == -1)
//						return NULL;
//				}

				if (PyList_Append((PyObject *) __pycookie, (PyObject *) __cookie) == -1)
					return NULL;

				Py_DECREF(__cookie);
//				Py_DECREF(__cn);
//				if (__cv != NULL)
//					Py_DECREF(__cv);
			}
			i++;
			Py_DECREF(__n);
			Py_DECREF(__v);
		}
		if (PyDict_SetItemString((PyObject *) __reqdict, "header", (PyObject *) __pylist) == -1)
			return NULL;

		if (PyDict_SetItemString((PyObject *) __reqdict, "cookie", (PyObject *) __pycookie) == -1)
			return NULL;

		Py_DECREF(__pylist);
		Py_DECREF(__pycookie);
	}

	/* accept encoding */
	PyListObject *__acenlist;
	if (request->encoding){
		int i=0;
		struct fly_bllist *__b;
		struct __fly_encoding *__e;

		__acenlist = (PyListObject *) PyList_New((Py_ssize_t) request->encoding->accept_count);
		fly_for_each_bllist(__b, &request->encoding->accepts){
			PyDictObject *__accepts = (PyDictObject *) PyDict_New();
			PyObject *__qv, *__name;
			__e = fly_bllist_data(__b, struct __fly_encoding, blelem);

			__qv = PyFloat_FromDouble(((double) __e->quality_value)/((double) 100.0));
			__name = PyUnicode_FromString(__e->type->name);

			if (PyDict_SetItemString((PyObject *) __accepts, "quality_value", (PyObject *) __qv) == -1)
				return NULL;
			if (PyDict_SetItemString((PyObject *) __accepts, "encoding_name", (PyObject *) __name) == -1)
				return NULL;

			Py_DECREF(__qv);
			Py_DECREF(__name);

			if (PyList_SetItem((PyObject *) __acenlist, i, (PyObject *) __accepts) == -1)
				return NULL;
			i++;
		}
	}else
		__acenlist = (PyListObject *) PyList_New((Py_ssize_t) 0);

	if (PyDict_SetItemString((PyObject *) __reqdict, "accept_encoding", (PyObject *) __acenlist) == -1)
		return NULL;

	Py_DECREF(__acenlist);

	/* accept language */
	PyListObject *__aclalist;
	if (request->language){
		int i=0;
		struct fly_bllist *__b;
		struct __fly_lang *__l;

		__aclalist = (PyListObject *) PyList_New((Py_ssize_t) request->language->lang_count);
		fly_for_each_bllist(__b, &request->language->langs){
			PyDictObject *__accepts = (PyDictObject *) PyDict_New();
			PyObject *__qv, *__name;
			__l = fly_bllist_data(__b, struct __fly_lang, blelem);

			__qv = PyFloat_FromDouble((double) __l->quality_value);
			__name = PyUnicode_FromString(fly_lang_name(__l));

			if (PyDict_SetItemString((PyObject *) __accepts, "quality_value", (PyObject *) __qv) == -1)
				return NULL;
			if (PyDict_SetItemString((PyObject *) __accepts, "language_name", (PyObject *) __name) == -1)
				return NULL;

			Py_DECREF(__qv);
			Py_DECREF(__name);

			if (PyList_SetItem((PyObject *) __aclalist, i, (PyObject *) __accepts) == -1)
				return NULL;
			i++;
		}
	}else
		__aclalist = (PyListObject *) PyList_New((Py_ssize_t) 0);

	if (PyDict_SetItemString((PyObject *) __reqdict, "accept_language", (PyObject *) __aclalist) == -1)
		return NULL;

	Py_DECREF(__aclalist);
	/* accept mime */
	PyListObject *__acmmlist;
	if (request->mime){
		int i=0;
		struct fly_bllist *__b;
		struct __fly_mime *__m;

		__acmmlist = (PyListObject *) PyList_New((Py_ssize_t) request->mime->accept_count);
		fly_for_each_bllist(__b, &request->mime->accepts){
			PyDictObject *__accepts = (PyDictObject *) PyDict_New();
			PyObject *__qv, *__tname, *__stname;
			__m = fly_bllist_data(__b, struct __fly_mime, blelem);

			__qv = PyFloat_FromDouble((double) __m->quality_value/(double) 100.0);
			__tname = PyUnicode_FromString(fly_mime_type(__m));
			if (fly_unlikely_null(__tname))
				goto response_500;

			__stname = PyUnicode_FromString(fly_mime_subtype(__m));
			if (fly_unlikely_null(__stname))
				goto response_500;

			if (PyDict_SetItemString((PyObject *) __accepts, "quality_value", (PyObject *) __qv) == -1)
				return NULL;
			if (PyDict_SetItemString((PyObject *) __accepts, "type_name", (PyObject *) __tname) == -1)
				return NULL;
			if (PyDict_SetItemString((PyObject *) __accepts, "subtype_name", (PyObject *) __stname) == -1)
				return NULL;

			Py_DECREF(__qv);
			Py_DECREF(__tname);
			Py_DECREF(__stname);

			if (PyList_SetItem((PyObject *) __acmmlist, i, (PyObject *) __accepts) == -1)
				return NULL;
			i++;
		}
	}else
		__acmmlist = (PyListObject *) PyList_New((Py_ssize_t) 0);

	if (PyDict_SetItemString((PyObject *) __reqdict, "accept_mime", (PyObject *) __acmmlist) == -1)
		return NULL;

	Py_DECREF(__acmmlist);

	/* accept charset */
	PyListObject *__accslist;
	if (request->mime){
		int i=0;
		struct fly_bllist *__b;
		struct __fly_charset *__c;

		__accslist = (PyListObject *) PyList_New((Py_ssize_t) request->charset->charset_count);
		fly_for_each_bllist(__b, &request->charset->charsets){
			PyDictObject *__accepts = (PyDictObject *) PyDict_New();
			PyObject *__qv, *__name;
			__c = fly_bllist_data(__b, struct __fly_charset, blelem);

			__qv = PyFloat_FromDouble((double) __c->quality_value);
			__name = PyUnicode_FromString(fly_charset_name(__c));

			if (PyDict_SetItemString((PyObject *) __accepts, "quality_value", (PyObject *) __qv) == -1)
				return NULL;
			if (PyDict_SetItemString((PyObject *) __accepts, "charset_name", (PyObject *) __name) == -1)
				return NULL;

			Py_DECREF(__qv);
			Py_DECREF(__name);

			if (PyList_SetItem((PyObject *) __accslist, i, (PyObject *) __accepts) == -1){
				return NULL;
			}
			i++;
		}
	}else
		__accslist = (PyListObject *) PyList_New((Py_ssize_t) 0);

	if (PyDict_SetItemString((PyObject *) __reqdict, "accept_charset", (PyObject *) __accslist) == -1)
		return NULL;

	Py_DECREF(__accslist);

	/* body */
	if (request->body){
		body = request->body;
		if (!body->multipart){
			PyObject *__pybody = PyBytes_FromStringAndSize(body->body, body->body_len);
			if (PyDict_SetItemString(__reqdict, "body", __pybody) == -1){
				Py_DECREF(__pybody);
				goto response_500;
			}

			Py_DECREF(__pybody);
		}else{
			struct fly_bllist *__b;
			Py_ssize_t i;
			PyObject *__pybody = PyList_New(body->multipart_count);
			if (__pybody == NULL)
				goto response_500;

			i=0;
			fly_for_each_bllist(__b, &body->multipart_parts){
				PyObject *__body_parts = PyDict_New();
				PyObject *__body_parts_ptr;
				PyObject *__body_headers;
				if (__body_parts == NULL)
					goto response_500;

				struct fly_body_parts *__p;
				__p = (struct fly_body_parts *) \
					  fly_bllist_data(__b, struct fly_body_parts, blelem);
				__body_headers = PyList_New(__p->header_count);
				if (__body_headers == NULL)
					goto response_500;

				struct fly_bllist *__bh;
				Py_ssize_t j=0;
				fly_for_each_bllist(__bh, &__p->headers){
					PyObject *__h = PyDict_New();
					PyObject *__bhn, *__bhv;
					struct fly_body_parts_header *__bph;

					__bph = (struct fly_body_parts_header *) \
						  fly_bllist_data(__bh, struct fly_body_parts_header, blelem);
					__bhn = PyUnicode_DecodeUTF8(__bph->name, __bph->name_len, NULL);
					if (__bhn == NULL)
						goto response_500;
					__bhv = PyUnicode_DecodeUTF8(__bph->value, __bph->value_len, NULL);
					if (__bhv == NULL)
						goto response_500;

					if (PyDict_SetItemString(__h, "name", __bhn) == -1)
						goto response_500;
					if (PyDict_SetItemString(__h, "value", __bhv) == -1)
						goto response_500;

					Py_DECREF(__bhn);
					Py_DECREF(__bhv);
					if (PyList_SetItem(__body_headers, j, __h) == -1)
						goto response_500;
					j++;
				}

				__body_parts_ptr = PyByteArray_FromStringAndSize(__p->ptr, __p->parts_len);
				if (__body_parts_ptr == NULL)
					goto response_500;

				if (PyDict_SetItemString(__body_parts, "content", __body_parts_ptr) == -1)
					goto response_500;
				if (PyDict_SetItemString(__body_parts, "header", __body_headers) == -1)
					goto response_500;


				Py_DECREF(__body_parts_ptr);
				Py_DECREF(__body_headers);
				if (PyList_SetItem(__pybody, i, __body_parts) == -1)
					goto response_500;
				i++;
			}

			if (PyDict_SetItemString(__reqdict, "body", __pybody) == -1){
				Py_DECREF(__pybody);
				goto response_500;
			}
			Py_DECREF(__pybody);
		}
	}

	__args = PyTuple_New(PYFLY_RESHANDLER_ARGS_COUNT);
	if (PyTuple_SetItem(__args, 0, __reqdict) == -1)
		return NULL;

	PyErr_Clear();
	/* call python function, and return response. */
	pyres = PyObject_CallObject(__func, __args);
	if (pyres == NULL || PyErr_Occurred()){
		PyErr_Clear();
		/* failure */
		goto response_500;
	}

	Py_DECREF(__func);
	Py_DECREF(__args);

	/* Python response --> C response */
	fly_context_t *ctx;
	ctx = request->ctx;
	res = fly_response_init(ctx);
	if (fly_unlikely_null(res))
		goto response_500;

	/*
	 * Allow type:
	 * str, subclass of _fly_response
	 */
	if (PyObject_IsSubclass((PyObject *) Py_TYPE(pyres), (PyObject *) &FlyResponseType)){

		PyObject *pyres_body, *pyres_header, *pyres_status_code, *pyres_content_type;
		pyres_body = PyObject_GetAttrString(pyres, PYFLY_BODY_KEY);

		/* HTTP HEAD type must not include body(content) */
		if (request_method == HEAD && pyres_body != NULL)
			goto response_500;
		else if (fly_unlikely_null(pyres_body))
			goto response_500;
		if (pyres_body != Py_None){
			char *body_ptr;
			ssize_t body_len;
			if (!PyBytes_Check(pyres_body))
				goto response_500;

			if (PyBytes_AsStringAndSize(pyres_body, &body_ptr, (Py_ssize_t *) &body_len) == -1){
				PyErr_Clear();
				goto response_500;
			}

			if (body_len > 0){
				/* over response body length */
				if (body_len > ctx->max_response_content_length)
					goto response_413;
				res->body = fly_body_init(ctx);
				res->body->body = fly_pballoc(res->body->pool, sizeof(fly_bodyc_t)*body_len);
				fly_body_setting(res->body, res->body->body, body_len);
				if (fly_unlikely_null(res->body->body))
					goto response_500;
				memcpy(res->body->body, body_ptr, body_len);
			}
		}
		Py_DECREF(pyres_body);

		/* set response header */
		Py_ssize_t pyres_hdr_len;
		pyres_header = PyObject_GetAttrString(pyres, PYFLY_HEADER_KEY);
		if (fly_unlikely_null(pyres_header))
			goto response_500;
		if (!PyList_CheckExact(pyres_header))
			goto response_500;

		fly_response_header_init(res, request);
		pyres_hdr_len = PyList_Size(pyres_header);
		if (pyres_hdr_len > 0){
			if (is_fly_request_http_v2(request))
				res->header->state = request->stream->state;
			if (fly_unlikely_null(res->header))
				goto response_500;

			for (Py_ssize_t i=0; i<pyres_hdr_len; i++){
				PyObject *pyhdr_c, *pyhdr_name, *pyhdr_value;
				const char *hdr_name, *hdr_value;
				ssize_t hdr_name_len, hdr_value_len;

				pyhdr_c = PyList_GET_ITEM(pyres_header, i);
				if (!PyDict_Check(pyhdr_c))
					goto response_500;

				pyhdr_name = PyDict_GetItemString(pyhdr_c, "name");
				if (fly_unlikely_null(pyhdr_name))
					goto response_500;
				if (!PyUnicode_Check(pyhdr_name))
					goto response_500;
				hdr_name = PyUnicode_AsUTF8AndSize(pyhdr_name, &hdr_name_len);
				if (is_fly_request_http_v2(request)){
					for (ssize_t i=0; i<hdr_name_len; i++)
						fly_alpha_upper_to_lower((char *) hdr_name+i);
				}

				pyhdr_value = PyDict_GetItemString(pyhdr_c, "value");
				if (fly_unlikely_null(pyhdr_value))
					goto response_500;
				if (!PyUnicode_Check(pyhdr_value))
					goto response_500;
				hdr_value = PyUnicode_AsUTF8AndSize(pyhdr_value, &hdr_value_len);

				if (hdr_name == NULL || hdr_value == NULL)
					goto response_500;

				if (fly_header_add_ver(res->header, (char *) hdr_name, hdr_name_len, (char *) hdr_value, hdr_value_len, is_fly_request_http_v2(request)) == -1)
					return NULL;

			}
		}

		Py_DECREF(pyres_header);

		/* set content type */
		pyres_content_type = PyObject_GetAttrString(pyres, PYFLY_CONTENT_TYPE_KEY);
		if (fly_unlikely_null(pyres_content_type))
			goto response_500;
		if (!PyUnicode_Check(pyres_content_type))
			goto response_500;
		const char *res_content_type;
		Py_ssize_t res_ctype_len;
		fly_mime_type_t *__mtype;
		res_content_type = PyUnicode_AsUTF8AndSize(pyres_content_type, &res_ctype_len);
		if (fly_unlikely_null(res_content_type))
			goto response_500;

		__mtype = fly_mime_type_from_str(res_content_type, (size_t) res_ctype_len);
		if (fly_unlikely_null(__mtype))
			goto response_500;
		if (fly_add_content_type(res->header, __mtype, is_fly_request_http_v2(request)) == -1)
			goto response_500;

		Py_DECREF(pyres_content_type);

		pyres_status_code = PyObject_GetAttrString(pyres, PYFLY_STATUS_CODE_KEY);
		if (fly_unlikely_null(pyres_status_code))
			goto response_500;
		if (!PyLong_Check(pyres_status_code))
			goto response_500;

		long __status_code = PyLong_AsLong(pyres_status_code);
		res->status_code = fly_status_code_from_long(__status_code);
		Py_DECREF(pyres_status_code);

		/* Check Python dictionary */
		Py_DECREF(pyres);
		return res;
	} else if (PyUnicode_Check(pyres)){
		const char *body_ptr;
		ssize_t body_len;

		/* HTTP HEAD type must not include body(content) */
		if (request_method == HEAD)
			goto response_500;

		if ((body_ptr=PyUnicode_AsUTF8AndSize(pyres, &body_len)) == NULL)
			goto response_500;

		if (body_len > 0){
			if (body_len > ctx->max_response_content_length)
				goto response_413;
			res->body = fly_body_init(ctx);
			res->body->body = fly_pballoc(res->body->pool, sizeof(fly_bodyc_t)*body_len);
			fly_body_setting(res->body, res->body->body, body_len);
			memcpy(res->body->body, body_ptr, body_len);
		}

		res->status_code = _200;
		Py_DECREF(pyres);
		return res;
	} else if (PyBytes_Check(pyres)){
		char *body_ptr;
		ssize_t body_len;

		if (PyBytes_AsStringAndSize(pyres, &body_ptr, (Py_ssize_t *) &body_len) == -1){
			PyErr_Clear();
			goto response_500;
		}

		if (body_len > 0){
			/* over response body length */
			if (body_len > ctx->max_response_content_length)
				goto response_413;
			res->body = fly_body_init(ctx);
			res->body->body = fly_pballoc(res->body->pool, sizeof(fly_bodyc_t)*body_len);
			fly_body_setting(res->body, res->body->body, body_len);
			if (fly_unlikely_null(res->body->body))
				goto response_500;
			memcpy(res->body->body, body_ptr, body_len);
		}
		res->status_code = _200;
		Py_DECREF(pyres);
		return res;
	} else if (pyres == Py_None){
		res->status_code = _200;
		Py_DECREF(pyres);
		return res;
	} else
		goto response_500;

	FLY_NOT_COME_HERE

response_413:
	Py_DECREF(pyres);
	fly_response_release(res);
	res = fly_413_response(request);
	return res;
response_500:
	if (pyres)
		Py_DECREF(pyres);
	fly_response_release(res);
	res = fly_500_response(request);
	return res;
}

static PyObject *__pyfly_mount_number(__pyfly_server_t *self, PyObject *args)
{
	const char *path;
	long mount_number;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "s", &path))
		return NULL;

	ctx = self->master->context;
	mount_number = (long) fly_mount_number(ctx->mount, path);
	if (mount_number == FLY_ENOTFOUND){
		PyErr_SetString(PyExc_ValueError, "invalid path.");
		return NULL;
	}

	return PyLong_FromLong(mount_number);
}

static int pyfly_mount(fly_context_t *ctx, const char *path)
{
	return fly_mount(ctx, path);
}

static PyObject *__pyfly_mount(__pyfly_server_t *self, PyObject *args)
{
	const char *__path;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "s", &__path))
		return NULL;

	ctx = self->master->context;
	if (pyfly_mount(ctx, __path) == -1)
		return NULL;

	Py_RETURN_NONE;
}

static PyObject *__pyfly_configure(__pyfly_server_t *self, PyObject *args)
{
	const char *config_path;
	Py_ssize_t routes_len;
	PyObject *routes;
	fly_master_t *master;
	fly_context_t *ctx;

	if (!PyArg_ParseTuple(args, "zO", &config_path, &routes))
		return NULL;

	if (routes)
		Py_INCREF(routes);

	if (!PyList_Check(routes)){
		PyErr_SetString(PyExc_ValueError, "Routes must be list type.");
		goto error;
	}

	if (config_path && setenv(FLY_CONFIG_PATH, config_path, 1) == -1){
		PyErr_SetString(PyExc_ValueError, "rnvironment setting error.");
		goto error;
	}

	/* config setting */
	switch (pyfly_parse_config_file()){
	case FLY_PARSE_CONFIG_NOTFOUND:
		break;
	case FLY_PARSE_CONFIG_ERROR:
		goto error;
	case FLY_PARSE_CONFIG_SUCCESS:
		break;
	case FLY_PARSE_CONFIG_SYNTAX_ERROR:
		goto error;
	default:
		goto error;
	}

	master = pyfly_master_init();
	if (master == NULL)
		goto error;

	if (fly_is_create_pidfile() && \
			pyfly_create_pidfile_noexit() == -1)
		goto error;

	ctx = master->context;
	if (pyfly_mount_init(ctx) < 0)
		goto error;

	routes_len = PyList_Size(routes);
	if (routes_len > 0){
		for (Py_ssize_t j=0; j<routes_len; j++){
			PyObject *__d, *uri, *func, *method;
			const char *uri_c, *method_c;
			Py_ssize_t uri_size;
			fly_method_e *m;

			__d = PyList_GetItem(routes, j);
			if (__d == NULL)
				goto error;

			if (!PyDict_Check(__d))
				goto error;

			uri = PyDict_GetItemString(__d, "uri");
			func = PyDict_GetItemString(__d, "func");
			method = PyDict_GetItemString(__d, "method");
			if (uri == NULL){
				PyErr_SetString(PyExc_ValueError, "route must have uri key.");
				goto error;
			}
			if (func == NULL){
				PyErr_SetString(PyExc_ValueError, "route must have func key.");
				goto error;
			}
			if (method == NULL){
				PyErr_SetString(PyExc_ValueError, "route must have method key.");
				goto error;
			}

			/* NULL termination */
			uri_c = PyUnicode_AsUTF8AndSize(uri, &uri_size);
			/* NULL termination */
			method_c = PyUnicode_AsUTF8AndSize(method, NULL);
			if (uri_c == NULL || method_c == NULL)
				goto error;

			m = fly_match_method_name_e((char *) method_c);
			if (m == NULL){
				PyErr_SetString(PyExc_ValueError, "fly method unmatch error");
				goto error;
			}

			if (!PyCallable_Check(func))
				goto error;

			struct fly_err err;
			if (fly_register_route(ctx->route_reg, pyfly_route_handler, (char *) uri_c, uri_size, *m, FLY_ROUTE_FLAG_PYTHON, (void *) func, &err) != FLY_REGISTER_ROUTE_SUCCESS){
				PyErr_Format(PyExc_ValueError, "Register route error. %s", err.content);
				goto error;
			}
		}
	}

	self->master = master;
	self->port = (long) fly_server_port();
	self->host = (const char *) fly_server_host();
	self->worker = (long) master->now_workers;
	self->reqworker = (long) master->req_workers;
	self->ssl = (bool) fly_ssl();

#ifdef DEBUG
	printf("%d\n", self->ssl);
	if (fly_ssl_crt_path())
		printf("%s\n", fly_ssl_crt_path());
	if (fly_ssl_key_path())
		printf("%s\n", fly_ssl_key_path());
	if (fly_log_path())
		printf("%s\n", fly_log_path());
#endif
	if (self->ssl && fly_ssl_crt_path() != NULL){
		char *__tmp;

		__tmp = realpath((const char *) fly_ssl_crt_path(), NULL);
		if (__tmp == NULL){
			PyErr_Format(PyExc_ValueError, "SSL certificate path: %s", strerror(errno));
			goto error;
		}
		self->ssl_crt_path = fly_pballoc(self->master->context->pool, sizeof(char)*(strlen(__tmp)+1));
		memset(self->ssl_crt_path, '\0', sizeof(char)*(strlen(__tmp)+1));
		memcpy(self->ssl_crt_path, __tmp, sizeof(char)*strlen(__tmp));
		free(__tmp);
	}else
		self->ssl_crt_path = NULL;

	if (self->ssl && fly_ssl_key_path() != NULL){
		char *__tmp;

		__tmp = realpath((const char *) fly_ssl_key_path(), NULL);
		if (__tmp == NULL){
			PyErr_Format(PyExc_ValueError, "SSL key path:%s", strerror(errno));
			goto error;
		}
		self->ssl_key_path = fly_pballoc(self->master->context->pool, sizeof(char)*(strlen(__tmp)+1));
		memset(self->ssl_key_path, '\0', sizeof(char)*(strlen(__tmp)+1));
		memcpy(self->ssl_key_path, __tmp, sizeof(char)*strlen(__tmp));
		free(__tmp);
	}else
		self->ssl_key_path = NULL;

	if (fly_log_path() != NULL){
		char *__tmp;

		__tmp = realpath((const char *) fly_log_path(), NULL);
		if (__tmp == NULL){
			PyErr_Format(PyExc_ValueError, "Log path: %s", strerror(errno));
			goto error;
		}

		self->log = fly_pballoc(self->master->context->pool, sizeof(char)*(strlen(__tmp)+1));
		memset(self->log, '\0', sizeof(char)*(strlen(__tmp)+1));
		memcpy(self->log, __tmp, sizeof(char)*strlen(__tmp));
		free(__tmp);
	}else
		self->log = NULL;

	Py_RETURN_NONE;

error:
	Py_DECREF(routes);
	return NULL;
}


static PyObject *__pyfly_run(__pyfly_server_t *self, PyObject *args)
{
	const char *reload_filepath;
	int daemon, reload;

	if (!PyArg_ParseTuple(args, "zpp", &reload_filepath, &reload, &daemon))
		return NULL;

	if (daemon){
		fprintf(stderr, "To be daemon process...");
		if (fly_daemon(self->master->context) == -1){
			PyErr_SetString(PyExc_RuntimeError, "to be daemon process error.");
			return NULL;
		}
	}
#ifdef DEBUG
	__log_test(self->master->context);
#endif
	fly_master_setreload(self->master, reload_filepath, (bool) reload);
	fly_master_worker_spawn(self->master, fly_worker_process);

	switch(fly_master_process(self->master)){
	case  FLY_MASTER_SIGNAL_END:
		return PyLong_FromLong((long) FLY_MASTER_SIGNAL_END);
	case  FLY_MASTER_RELOAD:
		return PyLong_FromLong((long) FLY_MASTER_RELOAD);
	default:
		FLY_NOT_COME_HERE
	}

	Py_RETURN_NONE;
}

static PyObject *__pyfly_mount_files(__pyfly_server_t *self, PyObject *args)
{
	int __mn, mount_files_count;
	fly_context_t *ctx;
	if (!PyArg_ParseTuple(args, "i", &__mn))
		return NULL;

	ctx = self->master->context;

	mount_files_count = fly_mount_files_count(ctx->mount, __mn);
	if (mount_files_count == -1){
		PyErr_SetString(PyExc_ValueError, "invalid mount number.");
		return NULL;
	}
	return PyLong_FromLong((long) mount_files_count);
}

static PyMethodDef __pyfly_server_methods[] = {
	{"_mount_number", (PyCFunction) __pyfly_mount_number, METH_VARARGS, ""},
	{"_mount", (PyCFunction) __pyfly_mount, METH_VARARGS, ""},
	{"_configure", (PyCFunction) __pyfly_configure, METH_VARARGS, ""},
	{"run", (PyCFunction) __pyfly_run, METH_VARARGS, ""},
	{"_mount_files", (PyCFunction) __pyfly_mount_files, METH_VARARGS, ""},
	{NULL}
};

static PyTypeObject __pyfly_server_type = {
	PyVarObject_HEAD_INIT(NULL, 0)
	.tp_name = PYFLY_SERVER_TYPE_NAME,
	.tp_doc = "",
	.tp_basicsize = sizeof(__pyfly_server_t),
	.tp_itemsize = 0,
	.tp_flags = Py_TPFLAGS_DEFAULT | Py_TPFLAGS_BASETYPE,
	.tp_new = PyType_GenericNew,
	.tp_init = (initproc) __pyfly_server_init,
	.tp_dealloc = (destructor) __pyfly_server_dealloc,
	.tp_methods = __pyfly_server_methods,
	.tp_base = &PyBaseObject_Type,
	.tp_members = __pyfly_server_members,
	.tp_getset = __pyfly_server_getsetters,
};

static PyModuleDef __pyfly_server_module = {
	PyModuleDef_HEAD_INIT,
	.m_name = PYFLY_SERVER_MODULE_NAME,
	.m_doc = "",
	.m_size = -1,
};

PyMODINIT_FUNC PyInit__fly_server(void)
{
	PyObject *m;

	if (PyType_Ready(&__pyfly_server_type) < 0)
		return NULL;
	if (PyType_Ready(&VersionObjectType) < 0)
		return NULL;
	if (PyType_Ready(&SchemeObjectType) < 0)
		return NULL;
	if (PyType_Ready(&FlyResponseType) < 0)
		return NULL;

	m = PyModule_Create(&__pyfly_server_module);
	if (m == NULL)
		return NULL;

	Py_INCREF(&__pyfly_server_type);
	if (PyModule_AddObject(m, "_fly_server", (PyObject *) &__pyfly_server_type) < 0){
		Py_DECREF(&__pyfly_server_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(&VersionObjectType);
	if (PyModule_AddObject(m, "_fly_version", (PyObject *) &VersionObjectType) < 0){
		Py_DECREF(&VersionObjectType);
		Py_DECREF(&__pyfly_server_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(&SchemeObjectType);
	if (PyModule_AddObject(m, "_fly_scheme", (PyObject *) &SchemeObjectType) < 0){
		Py_DECREF(&SchemeObjectType);
		Py_DECREF(&VersionObjectType);
		Py_DECREF(&__pyfly_server_type);
		Py_DECREF(m);
		return NULL;
	}
	Py_INCREF(&FlyResponseType);
	if (PyModule_AddObject(m, "_fly_response", (PyObject *) &FlyResponseType) < 0){
		Py_DECREF(&FlyResponseType);
		Py_DECREF(&SchemeObjectType);
		Py_DECREF(&VersionObjectType);
		Py_DECREF(&__pyfly_server_type);
		Py_DECREF(m);
		return NULL;
	}
	return m;
}
