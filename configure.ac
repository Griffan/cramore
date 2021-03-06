# Configure script for cramore, utilities for SAM/BAM/CRAM/VCF/BCF files.
#
#    Copyright (C) 2015,2017 Genome Research Ltd.
#
#    Author: Hyun Min Kang <hmkang@umich.edu>
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL
# THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
# FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
# DEALINGS IN THE SOFTWARE.

AC_PREREQ([2.63]) 
AC_INIT([cramore], m4_esyscmd_s([sh ./version.sh 2>/dev/null]), [hmkang@umich.edu])
AM_INIT_AUTOMAKE

AC_PROG_MAKE_SET
AC_PROG_CXX
AC_LANG(C++)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_MACRO_DIR([m4])

m4_include([m4/ax_with_htslib.m4])
m4_include([m4/hts_prog_cc_warnings.m4])

dnl Copyright notice to be copied into the generated configure script
AC_COPYRIGHT([Portions copyright (C) 2015,2017 Genome Research Ltd.

This configure script is free software: you are free to change and
redistribute it.  There is NO WARRANTY, to the extent permitted by law.])

AC_PROG_CC

AC_SYS_LARGEFILE

AX_WITH_HTSLIB
if test "$ax_cv_htslib" != yes; then
  AC_MSG_ERROR([HTSlib development files not found

cramore uses HTSlib to parse sequence-based file formats etc.  Building it
requires an unpacked HTSlib source tree (which will be built in conjunction
with bcftools) or a previously-installed HTSlib.  In either case you may
need to configure --with-htslib=DIR to locate the appropriate HTSlib.

FAILED.  You must supply an HTSlib in order to build bcftools successfully.])
fi

if test "$ax_cv_htslib_which" = source; then
  Hsource=
  Hinstall='#'
else
  Hsource='#'
  Hinstall=
fi
AC_SUBST([Hsource])
AC_SUBST([Hinstall])

if test $zlib_devel != ok; then
  AC_MSG_ERROR([zlib development files not found

cramore uses compression routines from the zlib library <http://zlib.net>.
Building bcftools requires zlib development files to be installed on the build
machine; you may need to ensure a package such as zlib1g-dev (on Debian or
Ubuntu Linux) or zlib-devel (on RPM-based Linux distributions) is installed.

FAILED.  This error must be resolved in order to build bcftools successfully.])
fi

AS_IF([test "x$hts_late_cflags" != x],[CFLAGS="$CFLAGS $hts_late_cflags"])

AC_CONFIG_FILES([config.mk])

AC_CHECK_HEADERS([limits.h stddef.h stdint.h stdlib.h string.h unistd.h])

AC_HEADER_STDBOOL
AC_C_INLINE
AC_TYPE_INT32_T
AC_TYPE_INT64_T
AC_C_RESTRICT
AC_TYPE_SIZE_T
AC_TYPE_UINT16_T
AC_TYPE_UINT32_T
AC_TYPE_UINT64_T
AC_TYPE_UINT8_T
AC_CHECK_TYPES([ptrdiff_t])

# Checks for library functions.
AC_FUNC_ERROR_AT_LINE
AC_FUNC_MALLOC
AC_HEADER_MAJOR
AC_FUNC_REALLOC
AC_FUNC_STRTOD
AC_CHECK_FUNCS([floor gethostname memset pow select sqrt strchr strstr strtol])
AC_CHECK_LIB(z, gzopen, [], [AC_MSG_ERROR([libz.{so,a} was not found. Please install zlib at http://www.zlib.net/ first])])
AC_CHECK_LIB([m],[erf],[AC_DEFINE([HAVE_ERF],[1],[libm includes erf])])
AC_CHECK_LIB([m],[erfc],[AC_DEFINE([HAVE_ERFC],[1],[libm includes erfc])])

AC_SUBST([include_htslib_mk], ["include \$(HTSDIR)/htslib.mk"])

#LT_PREREQ([2.2])
#LT_INIT([dlopen shared])

AC_CONFIG_FILES([Makefile])

AC_OUTPUT
