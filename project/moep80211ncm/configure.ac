AC_INIT([Moep80211 NCM], [0.0], [moepi@moepi.net], [moep80211ncm], [http://moep80211.net])
#AC_PREREQ([2.69])

AC_PROG_CC
AC_PROG_LN_S

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects foreign])
AM_SILENT_RULES([yes])

AM_PROG_AR
AM_PROG_CC_C_O	# Needed for some buggy automake versions

AC_SUBST(AR_FLAGS, [cr]) # Fixes ar warning during make

LT_INIT([disable-shared])

PKG_CHECK_MODULES([LIBMOEP], [libmoep >= 2.0])

AC_SEARCH_LIBS([exp], [m])
AC_SEARCH_LIBS([timer_create], [rt])
AC_SEARCH_LIBS([timer_delete], [rt])
AC_SEARCH_LIBS([timer_settime], [rt])
AC_SEARCH_LIBS([timer_gettime], [rt])

LIBMOEPCOMMON_CFLAGS="-I\$(top_srcdir)/libmoepcommon/include"
AC_SUBST(LIBMOEPCOMMON_CFLAGS)

LIBMOEPGF_CFLAGS="-I\$(top_srcdir)/libmoepgf/include"
AC_SUBST(LIBMOEPGF_CFLAGS)
LIBMOEPGF_LIBS="\$(top_builddir)/libmoepgf/libmoepgf.la"
AC_SUBST(LIBMOEPGF_LIBS)

LIBMOEPRLNC_CFLAGS="-I\$(top_srcdir)/libmoeprlnc/include"
AC_SUBST(LIBMOEPRLNC_CFLAGS)
LIBMOEPRLNC_LIBS="\$(top_builddir)/libmoeprlnc/libmoeprlnc.la"
AC_SUBST(LIBMOEPRLNC_LIBS)

LIBJSM_CFLAGS="-I\$(top_srcdir)/libjsm/include"
AC_SUBST(LIBJSM_CFLAGS)
LIBJSM_LIBS="\$(top_builddir)/libjsm/libjsm.la"
AC_SUBST(LIBJSM_LIBS)

AC_CONFIG_FILES([Makefile])

AC_CONFIG_SUBDIRS([libmoepgf libmoeprlnc libjsm])

AC_OUTPUT
