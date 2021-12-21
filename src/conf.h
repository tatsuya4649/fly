#ifndef _FLY_CONFIG_H
#define _FLY_CONFIG_H

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "util.h"

#define FLY_PREFIX			"./"
#define FLY_PID_FILE		(FLY_PREFIX "log/fly.pid")
#define FLY_PID_MAXSTRLEN		100
#define FLY_CONFIG_DEFAULT_PATH		"fly.conf"
#define FLY_CONFIG_PATH				"FLY_CONFIG_PATH"

#define FLY_CONFIG_BOOL_TRUE			"true"
#define FLY_CONFIG_BOOL_UPPER_TRUE		"True"
#define FLY_CONFIG_BOOL_FALSE			"false"
#define FLY_CONFIG_BOOL_UPPER_FALSE		"False"
struct fly_config{
	/* config file item name */
	const char *name;
	/* for environment variable */
	const char *env_name;
	const char *env_value;

#define FLY_CONFIG_INTEGER			(1<<0)
#define FLY_CONFIG_STRING			(1<<1)
#define FLY_CONFIG_BOOL				(1<<2)
	int flag;
};

extern struct fly_config configs[];
#define FLY_CONFIG(__n, __en, __ev, __f)		{__n, __en, __ev, __f}

#define FLY_PARSE_CONFIG_NOTFOUND			(-2)
#define FLY_PARSE_CONFIG_ERROR				(-1)
#define FLY_PARSE_CONFIG_SUCCESS			(1)
#define FLY_PARSE_CONFIG_SYNTAX_ERROR		(0)
int fly_parse_config_file(void);
long fly_config_value_long(char *name);
char *fly_config_value_str(char *name);
bool fly_config_value_bool(char *name);
#endif
