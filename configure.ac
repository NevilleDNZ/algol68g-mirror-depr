AC_INIT([algol-68-genie], [2.1.0], [Marcel van der Veer <algol68g@xs4all.nl>])
AM_INIT_AUTOMAKE([gnu])
AC_CONFIG_SRCDIR([a68g.h])
AC_CONFIG_HEADERS([a68g-config.h])
AC_CANONICAL_HOST

case "$host" in
    *86-*-gnu | *86_64-*-gnu | *86-*-linux* | *86_64-*-linux*) 
        AC_DEFINE(HAVE_LINUX,1,[Define this if LINUX was detected]) 
        AC_DEFINE(HAVE_IEEE_754,1,[Define this if IEEE_754 compliant]) 
        export=true
        ;;
    *86-*-*darwin* | *86_64-*-*darwin*) 
        AC_DEFINE(HAVE_MACOS_X,1,[Define this if DARWIN was detected]) 
        AC_DEFINE(HAVE_IEEE_754,1,[Define this if IEEE_754 compliant]) 
        export=false
        ;;
    *86-*-freebsd* | *86_64-*-freebsd*) 
        AC_DEFINE(HAVE_FREEBSD,1,[Define this if FreeBSD was detected]) 
        AC_DEFINE(HAVE_IEEE_754,1,[Define this if IEEE_754 compliant]) 
        export=true
	;;
    *86-*-openbsd* | *86_64-*-openbsd*) 
        AC_DEFINE(HAVE_OPENBSD,1,[Define this if OpenBSD was detected]) 
	AC_MSG_RESULT([WARNING: tested platform, but configuring interpreter-only!])
	;;
    *-*-*)
	AC_MSG_RESULT([WARNING: untested platform, therefore configuring interpreter-only!])
	;;
esac

AM_CONDITIONAL([EXPORT_DYNAMIC], [test x$export = xtrue])

# Checks for programs.
# AC_PROG_INSTALL
AC_PROG_MAKE_SET
AC_LANG(C)
AC_PROG_CC([gcc])
AM_PROG_CC_C_O
AC_PROG_CPP
AC_PROG_YACC
AM_PROG_LEX

if test "x$CC" = "xgcc"
then
        AC_DEFINE(HAVE_GCC,1,[Define this if GCC was detected]) 
fi

# Checks for typedefs, structures, and compiler characteristics.
AC_TYPE_UINT16_T
AC_C_VOLATILE
AC_C_CHAR_UNSIGNED

CFLAGS="$CFLAGS -I/usr/local/pgsql/include -I/opt/local/include"
CXXFLAGS="$CXXFLAGS -I/usr/local/pgsql/include -I/opt/local/include"
LDFLAGS="$LDFLAGS -L/usr/local/pgsql/lib -L/opt/local/lib"

# Checks for optional libraries.
AC_CHECK_LIB([dl], [dlopen])
AC_CHECK_LIB([m], [cos])
AC_CHECK_LIB([gslcblas], [cblas_dgemm])
AC_CHECK_LIB([gsl], [gsl_blas_dgemm])
AC_CHECK_LIB([ncurses], [initscr])
AC_CHECK_LIB([plot], [pl_openpl_r])
AC_CHECK_LIB([pq], [PQconnectdb])
AC_CHECK_LIB([pthread], [pthread_self])
AC_CHECK_LIB([termcap], [tgetnum])

# Checks for header files.
AC_CHECK_HEADERS([curses.h dirent.h dlfcn.h gsl/gsl_blas.h gsl/gsl_complex.h gsl/gsl_complex_math.h gsl/gsl_errno.h gsl/gsl_fft_complex.h gsl/gsl_integration.h gsl/gsl_linalg.h gsl/gsl_math.h gsl/gsl_matrix.h gsl/gsl_permutation.h gsl/gsl_sf.h gsl/gsl_vector.h libpq-fe.h netdb.h netinet/in.h plot.h pthread.h regex.h string.h strings.h sys/socket.h term.h])

AH_BOTTOM([/* In case of incomplete GSL installation - tant pis */
#if ! defined  HAVE_GSL_GSL_BLAS_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_COMPLEX_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_COMPLEX_MATH_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_ERRNO_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_FFT_COMPLEX_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_INTEGRATION_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_LINALG_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_MATH_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_MATRIX_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_PERMUTATION_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_SF_H
#undef HAVE_LIBGSL
#endif

#if ! defined  HAVE_GSL_GSL_VECTOR_H
#undef HAVE_LIBGSL
#endif])

AH_BOTTOM([/* Do we have a compiler? */

/* Linux has libdl */
#if (defined HAVE_LINUX && defined HAVE_GCC && defined HAVE_DLFCN_H && defined HAVE_LIBDL)
#define HAVE_COMPILER 1
#endif

/* MacOS X has libdl */
#if (defined HAVE_MACOS_X && defined HAVE_GCC && defined HAVE_DLFCN_H && defined HAVE_LIBDL)
#define HAVE_COMPILER 1
#endif

/* FreeBSD has libdl functionality in libc */
#if (defined HAVE_FREEBSD && defined HAVE_GCC && defined HAVE_DLFCN_H)
#define HAVE_COMPILER 1
#endif])

AH_BOTTOM([ /* Can we access the internet? */
#if (defined HAVE_NETDB_H && defined HAVE_NETINET_IN_H && defined HAVE_SYS_SOCKET_H)
#define HAVE_HTTP
#endif])

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
