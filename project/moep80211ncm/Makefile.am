
SUBDIRS = libmoepgf libmoeprlnc libjsm

AM_CFLAGS = -O2 -Wall

bin_PROGRAMS = ncm

ncm_SOURCES  = src/bcast.c
ncm_SOURCES += src/bcast.h
ncm_SOURCES += src/daemonize.c
ncm_SOURCES += src/daemonize.h
ncm_SOURCES += src/frametypes.h
ncm_SOURCES += src/generation.c
ncm_SOURCES += src/generation.h
ncm_SOURCES += src/global.h
ncm_SOURCES += src/ncm.c
ncm_SOURCES += src/ncm.h
ncm_SOURCES += src/neighbor.c
ncm_SOURCES += src/neighbor.h
ncm_SOURCES += src/linkstate.c
ncm_SOURCES += src/linkstate.h
ncm_SOURCES += src/lqe.c
ncm_SOURCES += src/lqe.h
ncm_SOURCES += src/params.h
ncm_SOURCES += src/ralqe.c
ncm_SOURCES += src/ralqe.h
ncm_SOURCES += src/qdelay.c
ncm_SOURCES += src/qdelay.h
ncm_SOURCES += src/session.c
ncm_SOURCES += src/session.h

ncm_CPPFLAGS  = $(LIBMOEP_CFLAGS)
ncm_CPPFLAGS += $(LIBMOEPCOMMON_CFLAGS)
ncm_CPPFLAGS += $(LIBMOEPGF_CFLAGS)
ncm_CPPFLAGS += $(LIBMOEPRLNC_CFLAGS)
ncm_CPPFLAGS += $(LIBJSM_CFLAGS)

ncm_LDADD  = $(LIBMOEP_LIBS)
ncm_LDADD += $(LIBMOEPGF_LIBS)
ncm_LDADD += $(LIBMOEPRLNC_LIBS)
ncm_LDADD += $(LIBJSM_LIBS)

noinst_HEADERS  = libmoepcommon/include/moepcommon/benchmark.h
noinst_HEADERS += libmoepcommon/include/moepcommon/list.h
noinst_HEADERS += libmoepcommon/include/moepcommon/list_sort.h
noinst_HEADERS += libmoepcommon/include/moepcommon/timeout.h
noinst_HEADERS += libmoepcommon/include/moepcommon/types.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/alignment.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/assertion.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/hexdump.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/log.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/mac.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/maths.h
noinst_HEADERS += libmoepcommon/include/moepcommon/util/timespec.h
