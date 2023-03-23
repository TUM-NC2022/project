
AM_CFLAGS = -O2 -Wall

AM_CPPFLAGS = -I$(top_srcdir)/include $(LIBMOEPGF_CFLAGS) $(LIBMOEPCOMMON_CFLAGS)


lib_LTLIBRARIES = libmoeprlnc.la

libmoeprlnc_la_SOURCES  = src/rlnc.c

libmoeprlnc_la_LIBADD = $(LIBMOEPGF_LIBS)

libmoeprlnc_la_LDFLAGS = -version-info 0:0:0

libmoeprlnc_la_includedir = $(includedir)/moeprlnc

libmoeprlnc_la_include_HEADERS  = include/moeprlnc/rlnc.h
