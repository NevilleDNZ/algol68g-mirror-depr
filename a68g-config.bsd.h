// Generic configuration file for BSD.

#if defined (BUILD_BSD)

#define HAVE_LIBPTHREAD 1
#define HAVE_PTHREAD_H 1
#define BUILD_PARALLEL_CLAUSE 1

#define a68_pid_t pid_t

#define HAVE_ISINF 1
#define HAVE_ISNAN 1
#define HAVE_ISFINITE 1

#undef HAVE_LONG_TYPES

// #undef HAVE_GSL
// #undef HAVE_GNU_PLOTUTILS
// #undef HAVE_QUADMATH

// #undef HAVE_GNU_MPFR
// #undef HAVE_CURSES
// #undef HAVE_MATHLIB

#define HAVE_REGEX_H

#if defined (HAVE_GSL)
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
#define HAVE_GSL_GSL_VERSION_H 1
#endif

#if defined (HAVE_GNU_PLOTUTILS)
#define HAVE_PLOT_H
#else
#undef HAVE_PLOT_H
#endif

#if defined (BUILD_HTTP)
#define HAVE_STDINT_H
#else
#undef HAVE_STDINT_H
#endif

#if defined (HAVE_CURSES)
#define HAVE_CURSES_H
#define HAVE_LIBNCURSES
#else
#undef HAVE_CURSES_H
#undef HAVE_LIBNCURSES
#endif

#undef HAVE_DLFCN_H
#undef HAVE_LIBPQ_FE_H
#undef HAVE_TERM_H

#define HAVE_STDINT_H

#define HAVE_STDARG_H
#define HAVE_STDLIB_H
#define HAVE_ERRNO_H
#define HAVE_ASSERT_H
#define HAVE_CTYPE_H
#define HAVE_DIRENT_H
#define HAVE_FCNTL_H
#define HAVE_FLOAT_H
#define HAVE_LIMITS_H
#define HAVE_MATH_H
#define HAVE_COMPLEX_H
#define HAVE_SETJMP_H
#define HAVE_SIGNAL_H
#define HAVE_STDIO_H
#define HAVE_STRING_H
#define HAVE_SYS_RESOURCE_H
#define HAVE_SYS_STAT_H
#define HAVE_SYS_TYPES_H
#define HAVE_SYS_WAIT_H
#define HAVE_TIME_H
#define HAVE_UNISTD_H
#define HAVE_WINSOCK_H

#endif // BUILD_OPENBSD

// Name of package
#define PACKAGE "algol68g"

// Define to the full name of this package.
#define PACKAGE_NAME "algol68g"

// Define to the one symbol short name of this package.
#define PACKAGE_TARNAME "algol68g"

// Define to the address where bug reports for this package should be sent.
#define PACKAGE_BUGREPORT "Marcel van der Veer <algol68g@xs4all.nl>"

// Define to the full name and version of this package.
#define PACKAGE_STRING "algol68g 3.0.0"

// Define to the version of this package.
#define PACKAGE_VERSION "3.0.0"

// Version number of package
#define VERSION "3.0.0"

