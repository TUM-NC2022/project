
AM_DISTCHECK_CONFIGURE_FLAGS = --enable-examples

AM_CFLAGS = -O2 -Wall -Wstrict-prototypes -Wmissing-prototypes -Wno-unused-function

lib_LTLIBRARIES = libmoep.la

libmoep_la_SOURCES  = src/dev.c
libmoep_la_SOURCES += src/frame.c
libmoep_la_SOURCES += src/interfaces.c
libmoep_la_SOURCES += src/interfaces.h
libmoep_la_SOURCES += src/list.h
libmoep_la_SOURCES += src/moep_hdr_ext.c
libmoep_la_SOURCES += src/moep_hdr_ext.h
libmoep_la_SOURCES += src/system.c
libmoep_la_SOURCES += src/util.h

libmoep_la_SOURCES += src/ieee80211/addr.c

libmoep_la_SOURCES += src/netlink/attr.h
libmoep_la_SOURCES += src/netlink/error.c
libmoep_la_SOURCES += src/netlink/error.h
libmoep_la_SOURCES += src/netlink/util.c
libmoep_la_SOURCES += src/netlink/util.h

libmoep_la_SOURCES += src/modules/moep80211/moep80211.c

libmoep_la_SOURCES += src/modules/moep8023/moep8023.c

libmoep_la_SOURCES += src/modules/ieee80211/ieee80211.c

libmoep_la_SOURCES += src/modules/ieee8023/ieee8023.c

libmoep_la_SOURCES += src/modules/radio/nl80211.c
libmoep_la_SOURCES += src/modules/radio/nl80211.h
libmoep_la_SOURCES += src/modules/radio/radio.c
libmoep_la_SOURCES += src/modules/radio/radiotap.c
libmoep_la_SOURCES += src/modules/radio/radiotap.h
libmoep_la_SOURCES += src/modules/radio/radiotap_parser.c
libmoep_la_SOURCES += src/modules/radio/radiotap_parser.h

libmoep_la_SOURCES += src/modules/eth/eth.c

libmoep_la_SOURCES += src/modules/tap/tap.c

libmoep_la_SOURCES += src/modules/unix/unix.c

libmoep_la_CPPFLAGS = -I$(top_srcdir)/include $(LIBNL_CFLAGS)

libmoep_la_LIBADD = $(LIBNL_LIBS)

libmoep_la_LDFLAGS = -version-info 2:0:0

libmoep_la_includedir = $(includedir)/moep

libmoep_la_include_HEADERS  = include/moep/dev.h
libmoep_la_include_HEADERS += include/moep/frame.h
libmoep_la_include_HEADERS += include/moep/ieee80211_addr.h
libmoep_la_include_HEADERS += include/moep/ieee80211_frametypes.h
libmoep_la_include_HEADERS += include/moep/ieee80211_radiotap.h
libmoep_la_include_HEADERS += include/moep/module.h
libmoep_la_include_HEADERS += include/moep/moep_hdr_ext.h
libmoep_la_include_HEADERS += include/moep/radiotap.h
libmoep_la_include_HEADERS += include/moep/system.h
libmoep_la_include_HEADERS += include/moep/types.h

libmoep_la_include_modulesdir = $(includedir)/moep/modules

libmoep_la_include_modules_HEADERS  = include/moep/modules/moep80211.h
libmoep_la_include_modules_HEADERS += include/moep/modules/moep8023.h
libmoep_la_include_modules_HEADERS += include/moep/modules/ieee80211.h
libmoep_la_include_modules_HEADERS += include/moep/modules/ieee8023.h
libmoep_la_include_modules_HEADERS += include/moep/modules/radio.h
libmoep_la_include_modules_HEADERS += include/moep/modules/eth.h
libmoep_la_include_modules_HEADERS += include/moep/modules/tap.h
libmoep_la_include_modules_HEADERS += include/moep/modules/unix.h


bin_PROGRAMS =

EXTRA_DIST =


dist_man_MANS =


if WANT_EXAMPLES
bin_PROGRAMS += examples/moepeval examples/ptm

examples_moepeval_SOURCES = examples/moepeval.c
examples_moepeval_LDADD = libmoep.la
examples_moepeval_CPPFLAGS = -I$(top_srcdir)/include

examples_ptm_SOURCES = examples/ptm.c
examples_ptm_LDADD = libmoep.la
examples_ptm_CPPFLAGS = -I$(top_srcdir)/include

dist_man_MANS += examples/moepeval.1
dist_man_MANS += examples/ptm.1
else
EXTRA_DIST += examples/moepeval.1
EXTRA_DIST += examples/ptm.1
endif


dist_noinst_DATA = doc/html

doc/html: doc

if HAVE_DOXYGEN
doc: Doxyfile $(libmoep_la_include_HEADERS) $(libmoep_la_include_modules_HEADERS)
	@echo "  DOXYGEN"
	@$(DOXYGEN)

clean-local: clean-doc
else
doc:

maintainer-clean-local: clean-doc
endif

.PHONY: clean-doc
clean-doc:
	-rm -rf doc


install-data-hook: doc/html
	mkdir -p "$(DESTDIR)$(docdir)"
	cp --no-preserve=ownership,mode -r $< "$(DESTDIR)$(docdir)"

uninstall-hook:
	-rm -rf "$(DESTDIR)$(docdir)"


pkgconfigdir = $(libdir)/pkgconfig
pkgconfig_DATA = libmoep.pc
