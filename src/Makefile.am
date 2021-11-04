lib_LTLIBRARIES = libfly.la

SOURCE_FILES := server.c response.c header.c alloc.c mount.c method.c version.c math.c request.c util.c connect.c body.c route.c fsignal.c mime.c encode.c log.c worker.c master.c ftime.c event.c context.c err.c charset.c lang.c cache.c scheme.c buffer.c rbtree.c ssl.c v2.c queue.c bllist.c conf.c uri.c

SOURCE_INCLUDE_FILES := $(SOURCE_FILES:%.c=%.h) char.h fly.h

libfly_la_SOURCES = $(SOURCE_FILES)
libfly_la_CFLAGS = $(CFLAGS)
noinst_HEADERS = $(SOURCE_INCLUDE_FILES)

ACLOCAL_AMFLAGS = -I m4
