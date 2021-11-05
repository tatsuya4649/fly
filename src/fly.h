#ifndef _FLY_H
#define _FLY_H

#define FLY_BIND_PORT       3333
#define FLY_BACK_LOG        4096
#define FLY_SERV_ADDR       "127.0.0.1"

#define FLY_ROOT_DIR		"."
#define __FLY_PATH_FROM_ROOT(p)	FLY_ROOT_DIR "/" # p
#define FLY_PATH_FROM_ROOT(p)	(FLY_ROOT_DIR "/" # p)

#endif
