dnl vim: set sw=4 sts=4 ts=4 noet ft=config foldmethod=marker foldmarker={{{,}}} :

dnl {{{ program, version
AC_PREREQ(2.59)
AC_INIT([src/main.c])

VERSION_MAJOR=0
VERSION_MINOR=1
VERSION_FULL="$VERSION_MAJOR.$VERSION_MINOR"
VERSION="$VERSION_FULL"

AC_SUBST([VERSION_MAJOR])
AC_SUBST([VERSION_MINOR])
AC_SUBST([VERSION_FULL])

AM_INIT_AUTOMAKE(afprint, [$VERSION_FULL])

dnl {{{ git revision
AC_MSG_CHECKING([for git head])
if test -d "${GIT_DIR:-${ac_top_srcdir:-./}/.git}" ; then
	GITHEAD=`git describe 2>/dev/null`
	if test -z ${GITHEAD} ; then
		GITHEAD=`git rev-parse --short HEAD`
	fi
	if test -n "`git diff-index -m --name-only HEAD`" ; then
		GITHEAD=${GITHEAD}-dirty
	fi
fi
AC_MSG_RESULT([$GITHEAD])
AC_SUBST([GITHEAD])
dnl }}}
dnl }}}

dnl {{{ toolchain checks
AC_PROG_CC_C99
if test x"$ac_cv_prog_cc_c99" = x"no"; then
	AC_MSG_ERROR([afprint requires a C compiler that supports ISO C99!])
fi
AC_PROG_INSTALL
AC_PROG_MAKE_SET
dnl }}}

dnl {{{ check for byte order
AC_C_BIGENDIAN
dnl }}}

dnl {{{ check for external libraries
PKG_CHECK_MODULES([libofa], [libofa],,
				  [AC_MSG_ERROR([afprint requires libofa])])
PKG_CHECK_MODULES([libsndfile], [sndfile],,
				  [AC_MSG_ERROR([afprint requires libsndfile])])
dnl }}}

dnl {{{ extra cflags for gcc
AFPRINT_CFLAGS=
if test x"$GCC" = x"yes"; then
	AFPRINT_CFLAGS="$AFPRINT_CFLAGS -Wall -pedantic"
fi
AC_SUBST([AFPRINT_CFLAGS])
dnl }}}

dnl {{{ output
AM_CONFIG_HEADER(config.h)
AC_OUTPUT(
		  Makefile
		  src/Makefile
		  )
dnl }}}
