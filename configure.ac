
AC_INIT([yass],[0.1.0],[vincent@legout.info])
AC_CONFIG_SRCDIR([libyass/yass.c])
AM_INIT_AUTOMAKE([foreign dist-bzip2])

AM_SILENT_RULES([yes])

AC_CONFIG_MACRO_DIR([m4])

AC_PROG_CC
AC_PROG_CXX

LT_INIT

PKG_CHECK_MODULES(CAIRO, [cairo])
PKG_CHECK_MODULES(JANSSON, [jansson])

have_manpages=no
AC_ARG_ENABLE(manpages, AS_HELP_STRING([--disable-manpages], [disable manpages]))
if test "x$enable_manpages" != "xno"; then
        have_manpages=yes
fi
AM_CONDITIONAL(ENABLE_MANPAGES, [test "$have_manpages" = "yes"])

have_cplex=yes
AC_ARG_ENABLE(cplex, AS_HELP_STRING([--enable-cplex], [enable cplex]))
if test "x$enable_cplex" != "xyes"; then
        have_cplex=no
fi
AM_CONDITIONAL(ENABLE_CPLEX, [test "$have_cplex" = "yes"])

AS_CASE([$host],
	[x86_64*-*-*], [AC_SUBST(CPLEX, [libamd64])],
	[i?86*-*-*], [AC_SUBST(CPLEX, [lib386])]
)

AC_CONFIG_HEADERS([config.h])
AC_CONFIG_FILES([
	Makefile
	data/Makefile
	draw/Makefile
	libyass/Makefile
	libyass/libyass.pc
	man/Makefile
	processors/Makefile
	schedulers/Makefile
	src/Makefile
	tests/Makefile
])

AC_OUTPUT
AC_MSG_RESULT([
	$PACKAGE $VERSION
	========

	prefix:			${prefix}
	sysconfdir:		${sysconfdir}
	libdir:			${libdir}
	includedir:		${includedir}

	compiler:		${CC}
	cflags:			${CFLAGS}
	ldflags:		${LDFLAGS}

	cplex:			${have_cplex}

	man pages:		${have_manpages}
])
