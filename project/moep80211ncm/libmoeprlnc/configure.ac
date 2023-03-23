AC_INIT([Randon Linear Network Coding], [0.0], [moepi@moepi.net], [libmoeprlnc], [http://moep80211.net])
#AC_PREREQ([2.69])

AC_PROG_CC
AC_PROG_LN_S

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects foreign])
AM_SILENT_RULES([yes])

AM_PROG_AR
AM_PROG_CC_C_O	# Needed for some buggy automake versions

AC_SUBST(AR_FLAGS, [cr]) # Fixes ar warning during make

LT_INIT

LIBMOEPGF_CFLAGS="-I\$(top_srcdir)/../libmoepgf/include"
AC_SUBST(LIBMOEPGF_CFLAGS)
LIBMOEPGF_LIBS="\$(top_builddir)/../libmoepgf/libmoepgf.la"
AC_SUBST(LIBMOEPGF_LIBS)

LIBMOEPCOMMON_CFLAGS="-I\$(top_srcdir)/../libmoepcommon/include"
AC_SUBST(LIBMOEPCOMMON_CFLAGS)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
