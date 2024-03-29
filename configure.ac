#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

m4_define([ppu_version], 1.1.16)

AC_PREREQ([2.69])
AC_INIT([servicelog], ppu_version, [hegdevasant@linux.ibm.com])
AC_CONFIG_AUX_DIR([config])
AC_CONFIG_MACRO_DIR([m4])
AM_INIT_AUTOMAKE([-Wall subdir-objects -Werror foreign])
AC_CONFIG_SRCDIR([src/servicelog.c])
AC_CONFIG_SRCDIR([man/servicelog.8])
AC_CONFIG_HEADERS([config/config.h])
LT_INIT

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL
AM_PROG_CC_C_O

# Checks for libraries.
AC_CHECK_LIB([servicelog], [servicelog_open servicelog_close])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h limits.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T
AC_C_INLINE
AC_FUNC_FORK
AC_FUNC_MALLOC
AC_PROG_CXX
AC_TYPE_PID_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T

AC_MSG_CHECKING([whether to enable test])
AC_ARG_WITH([test],
	    [AS_HELP_STRING([--with-test],
			    [compile test binaries [default=no]])],
			    [if test "$withval" = "yes"; then
				AC_MSG_RESULT([yes])
				AC_REQUIRE([AX_APPEND_COMPILE_FLAGS])
				AX_APPEND_COMPILE_FLAGS([-DSERVICELOG_TEST])
			     else
				AC_MSG_RESULT([no])
			     fi
			    ],
			    [AC_MSG_RESULT([no])])

# Checks for library functions.
AC_CHECK_FUNCS([memset strtol strcasecmp strchr strdup strerror strrchr strstr strtoul])

AC_CONFIG_FILES([Makefile servicelog.spec])
AC_OUTPUT
