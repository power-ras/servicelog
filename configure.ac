# Copyright (C) 2008, 2011 IBM
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU Library General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU Library General Public License for more details.
#
# You should have received a copy of the GNU Library General Public
# Licence along with this program; if not, see <http://www.gnu.org/licenses/>. 

#                                               -*- Autoconf -*-
# Process this file with autoconf to produce a configure script.

AC_PREREQ(2.59)
AC_INIT([servicelog],[1.1.11],[hegdevasant@in.ibm.com])
AC_CONFIG_SRCDIR([src/servicelog.c])
AC_CONFIG_SRCDIR([doc/servicelog.8])
AC_CONFIG_HEADER([src/config.h])
AM_INIT_AUTOMAKE
AC_PROG_LIBTOOL

GENERIC_MAJOR_VERSION=1
GENERIC_MINOR_VERSION=1
GENERIC_MICRO_VERSION=11

GENERIC_VERSION=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION.$GENERIC_MICRO_VERSION
GENERIC_RELEASE=$GENERIC_MAJOR_VERSION.$GENERIC_MINOR_VERSION
AC_SUBST(GENERIC_RELEASE)
AC_SUBST(GENERIC_VERSION)

VERSION=$GENERIC_VERSION

# Checks for programs.
AC_PROG_CC
AC_PROG_INSTALL

# Checks for libraries.
AC_CHECK_LIB([servicelog], [servicelog_open servicelog_close])

# Checks for header files.
AC_HEADER_STDC
AC_CHECK_HEADERS([stdlib.h string.h unistd.h])

# Checks for typedefs, structures, and compiler characteristics.
AC_C_CONST
AC_TYPE_SIZE_T

# Checks for library functions.
AC_CHECK_FUNCS([memset strtol])

AC_CONFIG_FILES([Makefile src/Makefile doc/Makefile servicelog.spec])
AC_OUTPUT
