dnl Process with autoconf to generate configure script   -*- sh -*-

AC_INIT([LyX],[2.5.0~devel],[lyx-devel@lists.lyx.org],[lyx])
AC_PRESERVE_HELP_ORDER
# Use ISO format only. The frontend needs to parse this
AC_SUBST(LYX_DATE, ["2025-05-25"])
AC_PREREQ([2.65])
AC_CONFIG_SRCDIR(src/main.cpp)
AC_CONFIG_HEADERS([config.h])

AC_CONFIG_AUX_DIR(config)

# First check the version
LYX_CHECK_VERSION
LYX_VERSION_SUFFIX
LYX_CHECK_QT6
# Check how the files should be packaged
AC_CANONICAL_TARGET
LYX_USE_PACKAGING
# We need to define these variables here and the no-define option of
# AM_INIT_AUTOMAKE above because we alter $PACKAGE in LYX_USE_PACKAGING.
AC_DEFINE_UNQUOTED(PACKAGE, "$PACKAGE", [Name of package])

dnl Default maintainer mode to true for development versions
if test "${enable_maintainer_mode+set}" != set; then
  enable_maintainer_mode=$lyx_devel_version
fi
AM_MAINTAINER_MODE
AM_SILENT_RULES([yes])
# Automake is pulling the historical V7 format. This tar format supports
# file names only up to 99 characters. tar-ustar selects the ustar format defined
# by POSIX 1003.1-1988. This format is believed to be old enough to be portable.
save_PACKAGE=$PACKAGE
AM_INIT_AUTOMAKE([foreign dist-xz no-define 1.14 tar-ustar subdir-objects])
PACKAGE=$save_PACKAGE

### Set the execute permissions of the various scripts correctly
for file in config/install-sh ; do
  chmod 755 ${srcdir}/${file}
done

# Find a suitable python interpreter
AM_PATH_PYTHON([3.8.0])

# Tools for creating libraries (note that we do not use libtool)
AM_PROG_AR
AC_PROG_RANLIB
dnl Recent debian/ubuntu (at least) have built 'ar' so that deterministic mode is the default.
dnl This means that it does not make sense to use the 'u' flag (default ARFLAGS is 'cru').
AC_SUBST([ARFLAGS], [cr])

### Check for a C++ compiler
dnl We have to do weird tricks so that autoconf does not touch CXXFLAGS even
dnl if it is not set. We do not use autoconf defaults.
lyx_has_CXXFLAGS=${CXXFLAGS+yes}
if ! test "$lyx_has_CXXFLAGS" = yes; then
  dnl set to a dummy value so that AC_PROG_CXX does not try to set CXXFLAGS
  CXXFLAGS=" "
fi
LYX_PROG_CXX
if ! test "$lyx_has_CXXFLAGS" = yes; then
  unset CXXFLAGS
fi
AC_LANG(C++)

### Objective-C compiler
AC_PROG_OBJC
_AM_DEPENDENCIES([OBJC])

### Add extra directories to check for libraries.
LYX_WITH_DIR([extra-lib],[extra library directory],extra_lib, NONE)
LYX_LOOP_DIR($lyx_cv_extra_lib,LYX_ADD_LIB_DIR(lyx_ldflags,$dir))
test ! x"$lyx_ldflags" = x && LDFLAGS="$lyx_ldflags $LDFLAGS"

### Add extra directories to check for include files.
LYX_WITH_DIR([extra-inc],[extra include directory],extra_inc, NONE)
LYX_LOOP_DIR($lyx_cv_extra_inc,LYX_ADD_INC_DIR(lyx_cppflags,$dir))
test ! x"$lyx_cppflags" = x && CPPFLAGS="$lyx_cppflags $CPPFLAGS"

### Add both includes and libraries
LYX_WITH_DIR([extra-prefix],[extra lib+include directory],extra_prefix, NONE, ${prefix})
LYX_LOOP_DIR($lyx_cv_extra_prefix,[
  LYX_ADD_INC_DIR(CPPFLAGS,$dir/include)
  LYX_ADD_LIB_DIR(LDFLAGS,$dir/lib)
])

### These are needed in windows
AC_CHECK_LIB(shlwapi, main, [LIBSHLWAPI=-lshlwapi])
AC_SUBST(LIBSHLWAPI)
AC_CHECK_LIB(psapi, main, [LIBPSAPI=-lpsapi])
AC_SUBST(LIBPSAPI)
AC_CHECK_LIB(gdi32, main)
AC_CHECK_LIB(ole32, main)

LYX_USE_INCLUDED_NOD

### we need to know the byte order for unicode conversions
AC_C_BIGENDIAN

# Nice to have when an assertion triggers
LYX_CHECK_CALLSTACK_PRINTING

# Needed for our char_type
AC_CHECK_SIZEOF(wchar_t)

# Taken from gettext, needed for libiconv
AC_CACHE_CHECK([for wchar_t], [gt_cv_c_wchar_t],
   [AC_COMPILE_IFELSE([AC_LANG_PROGRAM([[#include <stddef.h>
      wchar_t foo = (wchar_t)'\0';]], [[]])],[gt_cv_c_wchar_t=yes],[gt_cv_c_wchar_t=no])])
if test $gt_cv_c_wchar_t = yes; then
  AC_DEFINE([HAVE_WCHAR_T], [1], [Define if you have the 'wchar_t' type.])
  HAVE_WCHAR_T=1
else
  HAVE_WCHAR_T=0
fi
AC_SUBST([HAVE_WCHAR_T])

# Needed for Mingw-w64
AC_TYPE_LONG_LONG_INT

# egrep is used in lib/Makefile.am
AC_PROG_EGREP

### We need iconv for unicode support (Qt frontend requires it too)
LYX_USE_INCLUDED_ICONV

### check for compression support
LYX_USE_INCLUDED_ZLIB

### check whether we build and install the supplied dtl programs
LYX_BUILD_INCLUDED_DTL

### check for file magic support (currently optional)
AC_CHECK_HEADERS(magic.h,
 [AC_CHECK_LIB(magic, magic_open, [LIBS="$LIBS -lmagic"],
  LYX_WARNING([cannot find libmagic. Please check that the libmagic library
   is correctly installed on your system.
   Falling back to builtin file format detection.]))],
 [LYX_WARNING([cannot find magic.h. Please check that the libmagic library
   is correctly installed on your system.
   Falling back to builtin file format detection.])])

### setup the qt frontend.
dnl The code below is not in a macro, because this would cause big
dnl problems with the AC_REQUIRE contained in QT_DO_IT_ALL.
QT_DO_IT_ALL([5.9.4])
AC_SUBST([FRONTENDS_SUBDIRS], [qt])
FRONTEND_INFO="${FRONTEND_INFO}\
  Qt Frontend:\n\
      Qt version:\t   ${QTLIB_VERSION}\n"

# fix the value of the prefixes.
test "x$prefix" = xNONE && prefix=$default_prefix
test "x$exec_prefix" = xNONE && exec_prefix='${prefix}'
if echo $prefix |grep ' ' >/dev/null 2>/dev/null ; then
  LYX_WARNING([the installation prefix \"${prefix}\" contains a space, which
   causes problems with the Makefiles. The installation will be done in
   directory \"`pwd`/installprefix\" instead. Please move its contents to
   the right place after installation.])
  prefix=`pwd`/installprefix
fi

### Setup po directory
AM_NLS
if test $USE_NLS = "yes" ; then
  AC_DEFINE(ENABLE_NLS, 1,
    [Define to 1 if translation of program messages to the user's native language
   is requested.])dnl'
fi
AM_PO_SUBDIRS

# some standard header files
AC_HEADER_MAJOR
AC_CHECK_HEADERS(sys/time.h sys/types.h sys/select.h locale.h utime.h sys/utime.h)

# some standard types
AC_CHECK_TYPE(mode_t,[AC_DEFINE(HAVE_MODE_T, 1, [Define this to 1 if your compiler supports the mode_t type.])])
AC_TYPE_OFF_T
AC_TYPE_PID_T
AC_TYPE_SIZE_T
AC_TYPE_UID_T

LYX_CHECK_DEF(PATH_MAX, limits.h, [int n = PATH_MAX;])

AC_CHECK_FUNCS(chmod close _close fork getpid _getpid lockf lstat mkfifo open _open pclose _pclose popen _popen readlink putenv setenv strerror unsetenv)
# Check the form of mkdir()
AC_FUNC_MKDIR
AC_FUNC_SELECT_ARGTYPES

if test "$lyx_use_packaging" = "macosx" ; then
  LYX_CHECK_MACOS_DEPLOYMENT_TARGET
fi

LYX_CHECK_SPELL_ENGINES
LYX_USE_INCLUDED_MYTHES

LYX_CHECK_WITH_SAXON
LYX_CHECK_WITH_XSLT_SHEETS

lyx_client_subdir=true
dnl LIBS already contains some X extra libs that may interfere.
save_LIBS="$LIBS"
LIBS=
AC_CHECK_FUNCS(fcntl,
  [AC_SEARCH_LIBS([gethostbyname], [nsl])
   AC_SEARCH_LIBS([socket], [socket network], [],
     [AC_CHECK_LIB([socket], [socket], [LIBS="-lsocket -lnsl $LIBS"],
		[], [-lnsl])])],
  [lyx_client_subdir=false])
AC_SUBST(SOCKET_LIBS,$LIBS)
LIBS="$save_LIBS"
AM_CONDITIONAL(BUILD_CLIENT_SUBDIR, $lyx_client_subdir)

lyx_win_res=false;
case ${host} in
    *mingw*|*cygwin*) lyx_win_res=true;;
    *freebsd*) AC_SEARCH_LIBS(backtrace_symbols, [execinfo])
esac
if test "x$lyx_win_res" = "xtrue"; then
    AC_CHECK_TOOL(RC, windres,)
    if test -z "$RC"; then
      AC_MSG_ERROR([Could not find a resource compiler])
    fi
fi
AM_CONDITIONAL(LYX_WIN_RESOURCE, $lyx_win_res)
LYX_SET_VERSION_INFO


### Some information on what just happened
real_bindir=`eval "echo \`eval \"echo ${bindir}\"\`"`
real_pkgdatadir=`eval "echo \`eval \"echo \\\`eval \\\"echo ${pkgdatadir}\\\"\\\`\"\`"`
real_localedir=`eval "echo \`eval \"echo ${datadir}/locale\"\`"`
test -z "${lyx_included_libs}" && lyx_included_libs="(none)"
VERSION_INFO="Configuration\n\
  Host type:               ${host}\n\
  Special build flags:     ${lyx_flags}\n\
  Bundled libraries:       ${lyx_included_libs}\n\
  C++ Compiler:            ${CXX} ${CXX_VERSION}\n\
  C++ Compiler flags:      ${AM_CPPFLAGS} ${AM_CXXFLAGS}\n\
  C++ Compiler user flags: ${CPPFLAGS} ${CXXFLAGS}\n\
  Linker flags:            ${AM_LDFLAGS}\n\
  Linker user flags:       ${LDFLAGS}\n\
${FRONTEND_INFO}\
  Packaging:               ${lyx_use_packaging}\n\
  LyX binary dir:          ${real_bindir}\n\
  LyX files dir:           ${real_pkgdatadir}\n"

AC_SUBST(VERSION_INFO)
AC_SUBST(AM_CPPFLAGS)
AC_SUBST(AM_CXXFLAGS)
AC_SUBST(AM_LDFLAGS)
AC_SUBST(real_pkgdatadir)

## Some config.h stuff

AH_TOP([
/* -*- C++ -*- */
/*
 * \file config.h
 * This file is part of LyX, the document processor.
 * Licence details can be found in the file COPYING.
 *
 * This is the compilation configuration file for LyX.
 * It was generated by autoconfs configure.
 * You might want to change some of the defaults if something goes wrong
 * during the compilation.
 */

#ifndef _CONFIG_H
#define _CONFIG_H
])

AH_BOTTOM([
/************************************************************
 ** You should not need to change anything beyond this point */

#ifndef HAVE_STRERROR
#if defined(__cplusplus)
extern "C"
#endif
char * strerror(int n);
#endif

#ifdef __CYGWIN__
#  define _DEFAULT_SOURCE
#  define NOMINMAX
#endif

#if defined(HAVE_WCHAR_T) && SIZEOF_WCHAR_T == 4
#  define USE_WCHAR_T
#endif


#endif
])

MSYS_AC_CANONICAL_PATH(lyx_abs_top_srcdir, ${srcdir})
MSYS_AC_CANONICAL_PATH(lyx_abs_installed_localedir, ${real_localedir})
MSYS_AC_CANONICAL_PATH(lyx_abs_installed_datadir, ${real_pkgdatadir})

AC_DEFINE_UNQUOTED([LYX_ABS_TOP_SRCDIR],
	"${lyx_abs_top_srcdir}", [Top source directory])
AC_DEFINE_UNQUOTED([LYX_ABS_INSTALLED_LOCALEDIR],
	"${lyx_abs_installed_localedir}",[Hard coded locale directory])
AC_DEFINE_UNQUOTED([LYX_ABS_INSTALLED_DATADIR],
	"${lyx_abs_installed_datadir}",[Hard system support directory])
AC_DEFINE_UNQUOTED([PROGRAM_SUFFIX],
	"${version_suffix}",[Program version suffix])

AC_DEFINE_UNQUOTED([LYX_DATE],"$LYX_DATE",[Date of release])
AC_DEFINE_UNQUOTED([VERSION_INFO],"$VERSION_INFO",[Full version info])
AC_DEFINE_UNQUOTED([LYX_DIR_VER],"$lyx_dir_ver",[Versioned env var for system dir])
AC_DEFINE_UNQUOTED([LYX_USERDIR_VER],"$lyx_userdir_ver",[Versioned env var for user dir])
AC_DEFINE_UNQUOTED([LYX_MAJOR_VERSION],$lyx_major,[Major version number])
AC_DEFINE_UNQUOTED([LYX_MINOR_VERSION],$lyx_minor,[Minor version number])
AC_DEFINE_UNQUOTED([LYX_RELEASE_LEVEL],$lyx_release,[Release version number])
AC_DEFINE_UNQUOTED([LYX_RELEASE_PATCH],$lyx_patch,[Patch version number])

AC_CONFIG_FILES([Makefile \
      lyx.1:lyx.1in \
      3rdparty/Makefile \
      3rdparty/dtl/Makefile \
      3rdparty/hunspell/Makefile \
      3rdparty/mythes/Makefile \
      3rdparty/nod/Makefile \
      3rdparty/libiconv/Makefile \
      $ICONV_ICONV_H_IN \
      3rdparty/zlib/Makefile \
      autotests/Makefile \
      config/Makefile \
      development/Makefile \
      development/MacOSX/Makefile \
      development/MacOSX/Info.plist \
      development/MacOSX/lyxrc.dist \
      development/MacOSX/spotlight/Makefile \
      development/cygwin/Makefile \
      development/cygwin/lyxrc.dist \
      development/lyx.spec \
      lib/lyx.desktop-temp:lib/lyx.desktop.in
      lib/Makefile \
      lib/doc/Makefile \
      lib/lyx2lyx/lyx2lyx_version.py \
      lib/lyx2lyx/Makefile \
      m4/Makefile \
      po/Makefile.in \
      sourcedoc/Doxyfile \
      sourcedoc/Makefile \
      src/client/Makefile \
      src/client/lyxclient.1:src/client/lyxclient.1in \
      src/Makefile \
      src/tex2lyx/Makefile \
      src/tex2lyx/tex2lyx.1:src/tex2lyx/tex2lyx.1in \
      src/convert/Makefile \
      src/support/Makefile \
      src/frontends/Makefile \
      src/frontends/qt/Makefile
])


AC_OUTPUT

# show version information
echo
printf "$VERSION_INFO"
echo

# Display a final warning if there has been a LYX_WARNING
LYX_CHECK_WARNINGS

cat <<EOF
Configuration of LyX was successful.
Type 'make' to compile the program,
and then 'make install' to install it.
EOF
