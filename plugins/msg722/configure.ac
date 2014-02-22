dnl Process this file with autoconf to produce a configure script.
AC_INIT([msg722],[0.0.0])
AC_CONFIG_SRCDIR([src/msg722.c])
AC_CANONICAL_HOST
AM_INIT_AUTOMAKE
LT_PREREQ([2.2.6])
LT_INIT
AC_SUBST([LIBTOOL_DEPS])
AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

dnl Checks for programs.
AC_PROG_CC
AC_DISABLE_STATIC
PKG_PROG_PKG_CONFIG([0.22])

dnl Checks for header files.
AC_HEADER_STDC

dnl Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_C_INLINE
AC_HEADER_TIME

PKG_CHECK_MODULES(MEDIASTREAMER, mediastreamer >= 2.1.0)
AC_SUBST(MEDIASTREAMER_CFLAGS)
AC_SUBST(MEDIASTREAMER_LIBS)

dnl define path of plugins:
PACKAGE_PLUGINS_DIR=mediastreamer/plugins
AC_SUBST(PACKAGE_PLUGINS_DIR)

AC_CONFIG_FILES(
Makefile
src/Makefile
)

AC_OUTPUT
