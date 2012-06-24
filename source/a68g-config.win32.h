#define HAVE_WIN32

#if defined HAVE_WIN32
#define HAVE_ASSERT_H
#define HAVE_CONIO_H
#define HAVE_CTYPE_H
#define HAVE_CURSES_H
#define HAVE_DIRENT_H
#define HAVE_FCNTL_H
#define HAVE_FLOAT_H
#define HAVE_HTTP
#define HAVE_LIBGSL
#define HAVE_LIBNCURSES
#define HAVE_LIBPLOT
#define HAVE_LIMITS_H
#define HAVE_MATH_H
#define HAVE_PLOT_H
#define HAVE_REGEX_H
#define HAVE_SETJMP_H
#define HAVE_SIGNAL_H
#define HAVE_STDIO_H
#define HAVE_STRING_H
#define HAVE_SYS_STAT_H
#define HAVE_SYS_TYPES_H
#define HAVE_TIME_H
#define HAVE_UNISTD_H
#define HAVE_WINSOCK_H

#define HAVE_GSL_GSL_BLAS_H 1
#define HAVE_GSL_GSL_COMPLEX_H 1
#define HAVE_GSL_GSL_COMPLEX_MATH_H 1
#define HAVE_GSL_GSL_ERRNO_H 1
#define HAVE_GSL_GSL_FFT_COMPLEX_H 1
#define HAVE_GSL_GSL_INTEGRATION_H 1
#define HAVE_GSL_GSL_LINALG_H 1
#define HAVE_GSL_GSL_MATH_H 1
#define HAVE_GSL_GSL_MATRIX_H 1
#define HAVE_GSL_GSL_PERMUTATION_H 1
#define HAVE_GSL_GSL_SF_H 1
#define HAVE_GSL_GSL_VECTOR_H 1

#define HAVE_PLOT_H

#define HAVE_GNU_GSL
#define HAVE_GNU_PLOTUTILS

#undef HAVE_COMPILER
#undef HAVE_DLFCN_H
#undef HAVE_LIBPQ_FE_H
#undef HAVE_PARALLEL_CLAUSE
#undef HAVE_PTHREAD_H
#undef HAVE_TERM_H

typedef unsigned __off_t;
extern int finite (double);
#define S_IRGRP (0x040)
#define S_IROTH (0x004)
#if (defined __MSVCRT__ && defined _environ)
#undef _environ
#endif
#endif /* defined HAVE_WIN32 */

/* Name of package */
#define PACKAGE "algol68g"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "Marcel van der Veer <algol68g@xs4all.nl>"

/* Define to the full name of this package. */
#define PACKAGE_NAME "algol68g"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "algol68g 2.4.1"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "algol68g"

/* Define to the version of this package. */
#define PACKAGE_VERSION "2.4.1"

/* Version number of package */
#define VERSION "2.4.1"

