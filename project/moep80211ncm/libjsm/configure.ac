AC_INIT([Jitter Suppression Module], [0.0], [martin.jobst@tum.de], [libjsm], [http://moep80211.net])
#AC_PREREQ([2.69])

AC_PROG_CC
AC_PROG_LN_S

AM_INIT_AUTOMAKE([-Wall -Werror subdir-objects foreign])
AM_SILENT_RULES([yes])

AM_PROG_AR
AM_PROG_CC_C_O	# Needed for some buggy automake versions

AC_SUBST(AR_FLAGS, [cr]) # Fixes ar warning during make

LT_INIT

LIBMOEPCOMMON_CFLAGS="-I\$(top_srcdir)/../libmoepcommon/include"
AC_SUBST(LIBMOEPCOMMON_CFLAGS)

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
