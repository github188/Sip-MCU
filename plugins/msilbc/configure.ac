# process this file with autoconf to produce a configure script.
AC_INIT([msilbc],[2.0.3])

# require at least this autoconf version
AC_PREREQ(2.63)

# set up the directory structure
AC_CONFIG_SRCDIR([ilbc.c])
AC_CONFIG_AUX_DIR([build-aux])
AC_CONFIG_MACRO_DIR([m4])

# declare this variable precious for automated processing from higher level
AC_ARG_VAR(MSILBC_CFLAGS,
	[Extra C compiler flags for msilbc, such as -DUSE_20MS_FRAMES=1])

# make a simpler command-line option available, too
AC_ARG_ENABLE([20ms-frames], [AS_HELP_STRING([--enable-20ms-frames],
			     [enable 20ms frame size (default is 30ms)])],
			     [MSILBC_CFLAGS="-DUSE_20MS_FRAMES=1"])

# initialize automake
AM_INIT_AUTOMAKE([tar-ustar])

# initialize libtool
AC_LIBTOOL_WIN32_DLL
LT_INIT([disable-static])

case $target_os in 
	*mingw*)
		mingw_found=yes
	;;
esac

AM_CONDITIONAL(BUILD_WIN32, test x$mingw_found = xyes)

AC_CONFIG_COMMANDS([libtool-hacking],[
if test "$mingw_found" = "yes" ; then
	AC_MSG_NOTICE([Hacking libtool to work with mingw...])
	sed -e 's/\*\" \$a_deplib \"\*/\*/' < ./libtool > libtool.tmp
	cp -f ./libtool.tmp ./libtool
	rm -f ./libtool.tmp
fi
],[mingw_found=$mingw_found])

# check for libraries that have pkg-config files installed
PKG_CHECK_MODULES(ORTP, ortp >= 0.16.0)
PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.0.0)
PKG_CHECK_MODULES(ILBC, ilbc >= 0.0.0)

# create the following files from their .in counterparts
AC_CONFIG_FILES([
  Makefile
  msilbc.iss
])
AC_OUTPUT

