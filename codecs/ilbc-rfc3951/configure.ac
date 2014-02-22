#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.


AC_PREREQ(2.59)
AC_INIT(ilbc,rfc3951)
AC_CONFIG_SRCDIR([ilbc/anaFilter.c])

AM_INIT_AUTOMAKE($PACKAGE_NAME,$PACKAGE_VERSION,no-define)

# Checks for programs.
AC_PROG_CC
AC_PROG_LIBTOOL
AC_ENABLE_SHARED(yes)
AC_ENABLE_STATIC(no)

# Checks for libraries.

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST

# Checks for library functions.
AC_CHECK_FUNCS([memmove memset pow sqrt])

AC_OUTPUT(
Makefile
ilbc/Makefile
platform/Makefile
platform/vsnet-ilbc/Makefile
doc/Makefile
)
