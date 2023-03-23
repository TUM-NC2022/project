
AM_CFLAGS = -O2 -Wall

AM_CPPFLAGS = -I$(top_srcdir)/include $(LIBMOEPCOMMON_CFLAGS)


lib_LTLIBRARIES = libjsm.la

libjsm_la_SOURCES  = src/jsm.c
libjsm_la_SOURCES += src/pdvstat.h
libjsm_la_SOURCES += src/ringbuffer.h
libjsm_la_SOURCES += src/timeutil.h

libjsm_la_LDFLAGS = -version-info 0:0:0

libjsm_la_includedir = $(includedir)

libjsm_la_include_HEADERS  = include/jsm.h
