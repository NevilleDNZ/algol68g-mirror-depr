/* a68g-config.h.  Generated from a68g-config.h.in by configure.  */
/* a68g-config.h.in.  Generated from configure.ac by autoheader.  */

/* Define this if OpenBSD was detected */
/* #undef BUILD_BSD */

/* Define this if CYGWIN was detected */
/* #undef BUILD_CYGWIN */

/* Define this if HAIKU was detected */
/* #undef BUILD_HAIKU */

/* Define this if LINUX was detected */
#define BUILD_LINUX 1

/* Define this if a good pthread installation was detected */
/* #undef BUILD_PARALLEL_CLAUSE */

/* Name of C compiler detected */
#define C_COMPILER "gcc"

/* Define to 1 if `TIOCGWINSZ' requires <sys/ioctl.h>. */
#define GWINSZ_IN_SYS_IOCTL 1

/* Define to 1 if you have the `aligned_alloc' function. */
#define HAVE_ALIGNED_ALLOC 1

/* Define to 1 if you have the <assert.h> header file. */
#define HAVE_ASSERT_H 1

/* Define to 1 if you have the `basename' function. */
#define HAVE_BASENAME 1

/* Define to 1 if you have the <complex.h> header file. */
#define HAVE_COMPLEX_H 1

/* Define to 1 if you have the `csqrt' function. */
#define HAVE_CSQRT 1

/* Define to 1 if you have the <ctype.h> header file. */
#define HAVE_CTYPE_H 1

/* Define this if curses was detected */
/* #undef HAVE_CURSES */

/* Define to 1 if you have the <curses.h> header file. */
/* #undef HAVE_CURSES_H */

/* Define to 1 if you have the <dirent.h> header file, and it defines `DIR'.
   */
#define HAVE_DIRENT_H 1

/* Define this if a good DL installation was detected */
/* #undef HAVE_DL */

/* Define to 1 if you have the <dlfcn.h> header file. */
/* #undef HAVE_DLFCN_H */

/* Define to 1 if you have the <errno.h> header file. */
#define HAVE_ERRNO_H 1

/* Define to 1 if you have the `exit' function. */
#define HAVE_EXIT 1

/* Define this if EXPORT_DYNAMIC is recognised */
#define HAVE_EXPORT_DYNAMIC 1

/* Define to 1 if you have the <fcntl.h> header file. */
#define HAVE_FCNTL_H 1

/* Define to 1 if you have the <fenv.h> header file. */
#define HAVE_FENV_H 1

/* Define if finite() is available */
#define HAVE_FINITE 1

/* Define to 1 if you have the <float.h> header file. */
#define HAVE_FLOAT_H 1

/* Define to 1 if you have the `fprintf' function. */
#define HAVE_FPRINTF 1

/* Define to 1 if you have the `free' function. */
#define HAVE_FREE 1

/* Define this if GCC was detected */
#define HAVE_GCC 1

/* Define this if a recent GNU MPFR installation was detected */
/* #undef HAVE_GNU_MPFR */

/* Define this if a good GNU plotutils installation was detected */
/* #undef HAVE_GNU_PLOTUTILS */

/* Define this if a good GNU GSL installation was detected */
/* #undef HAVE_GSL */

/* Define to 1 if you have the <gsl/gsl_blas.h> header file. */
/* #undef HAVE_GSL_GSL_BLAS_H */

/* Define to 1 if you have the <gsl/gsl_complex.h> header file. */
/* #undef HAVE_GSL_GSL_COMPLEX_H */

/* Define to 1 if you have the <gsl/gsl_complex_math.h> header file. */
/* #undef HAVE_GSL_GSL_COMPLEX_MATH_H */

/* Define to 1 if you have the <gsl/gsl_eigen.h> header file. */
/* #undef HAVE_GSL_GSL_EIGEN_H */

/* Define to 1 if you have the <gsl/gsl_errno.h> header file. */
/* #undef HAVE_GSL_GSL_ERRNO_H */

/* Define to 1 if you have the <gsl/gsl_fft_complex.h> header file. */
/* #undef HAVE_GSL_GSL_FFT_COMPLEX_H */

/* Define to 1 if you have the <gsl/gsl_integration.h> header file. */
/* #undef HAVE_GSL_GSL_INTEGRATION_H */

/* Define to 1 if you have the <gsl/gsl_linalg.h> header file. */
/* #undef HAVE_GSL_GSL_LINALG_H */

/* Define to 1 if you have the <gsl/gsl_math.h> header file. */
/* #undef HAVE_GSL_GSL_MATH_H */

/* Define to 1 if you have the <gsl/gsl_matrix.h> header file. */
/* #undef HAVE_GSL_GSL_MATRIX_H */

/* Define to 1 if you have the <gsl/gsl_permutation.h> header file. */
/* #undef HAVE_GSL_GSL_PERMUTATION_H */

/* Define to 1 if you have the <gsl/gsl_sf.h> header file. */
/* #undef HAVE_GSL_GSL_SF_H */

/* Define to 1 if you have the <gsl/gsl_statistics.h> header file. */
/* #undef HAVE_GSL_GSL_STATISTICS_H */

/* Define to 1 if you have the <gsl/gsl_vector.h> header file. */
/* #undef HAVE_GSL_GSL_VECTOR_H */

/* Define to 1 if you have the <gsl/gsl_version.h> header file. */
/* #undef HAVE_GSL_GSL_VERSION_H */

/* Define this if IEEE_754 compliant */
#define HAVE_IEEE_754 1

/* Define to 1 if you have the <inttypes.h> header file. */
#define HAVE_INTTYPES_H 1

/* Define if isfinite() is available */
#define HAVE_ISFINITE 1

/* Define if isinf() is available */
#define HAVE_ISINF 1

/* Define if isnan() is available */
#define HAVE_ISNAN 1

/* Define to 1 if you have the <libgen.h> header file. */
#define HAVE_LIBGEN_H 1

/* Define to 1 if you have the `gmp' library (-lgmp). */
/* #undef HAVE_LIBGMP */

/* Define to 1 if you have the `gsl' library (-lgsl). */
/* #undef HAVE_LIBGSL */

/* Define to 1 if you have the `gslcblas' library (-lgslcblas). */
/* #undef HAVE_LIBGSLCBLAS */

/* Define to 1 if you have the `m' library (-lm). */
#define HAVE_LIBM 1

/* Define to 1 if you have the `mpfr' library (-lmpfr). */
/* #undef HAVE_LIBMPFR */

/* Define to 1 if you have the `ncurses' library (-lncurses). */
/* #undef HAVE_LIBNCURSES */

/* Define to 1 if you have the `plot' library (-lplot). */
/* #undef HAVE_LIBPLOT */

/* Define to 1 if you have the `pq' library (-lpq). */
/* #undef HAVE_LIBPQ */

/* Define to 1 if you have the <libpq-fe.h> header file. */
/* #undef HAVE_LIBPQ_FE_H */

/* Define to 1 if you have the `pthread' library (-lpthread). */
/* #undef HAVE_LIBPTHREAD */

/* Define to 1 if you have the `quadmath' library (-lquadmath). */
/* #undef HAVE_LIBQUADMATH */

/* Define to 1 if you have the `readline' library (-lreadline). */
/* #undef HAVE_LIBREADLINE */

/* Define to 1 if you have the `Rmath' library (-lRmath). */
/* #undef HAVE_LIBRMATH */

/* Define to 1 if you have the `root' library (-lroot). */
/* #undef HAVE_LIBROOT */

/* Define to 1 if you have the `tic' library (-ltic). */
/* #undef HAVE_LIBTIC */

/* Define to 1 if you have the `tinfo' library (-ltinfo). */
/* #undef HAVE_LIBTINFO */

/* Define to 1 if you have the <limits.h> header file. */
#define HAVE_LIMITS_H 1

/* Define to 1 if you have the `longjmp' function. */
#define HAVE_LONGJMP 1

/* Define this if a good INT*8/REAL*10/REAL*16 installation was detected */
/* #undef HAVE_LONG_TYPES */

/* Define to 1 if you have the `malloc' function. */
#define HAVE_MALLOC 1

/* Define this if a good mathlib installation was detected */
/* #undef HAVE_MATHLIB */

/* Define to 1 if you have the <math.h> header file. */
#define HAVE_MATH_H 1

/* Define to 1 if you have the `memcpy' function. */
#define HAVE_MEMCPY 1

/* Define to 1 if you have the `memmove' function. */
#define HAVE_MEMMOVE 1

/* Define to 1 if you have the <memory.h> header file. */
#define HAVE_MEMORY_H 1

/* Define to 1 if you have the `memset' function. */
#define HAVE_MEMSET 1

/* Define to 1 if you have the <mpfr.h> header file. */
/* #undef HAVE_MPFR_H */

/* Define to 1 if you have the <ncurses/curses.h> header file. */
/* #undef HAVE_NCURSES_CURSES_H */

/* Define to 1 if you have the <ndir.h> header file, and it defines `DIR'. */
/* #undef HAVE_NDIR_H */

/* Define to 1 if you have the <netdb.h> header file. */
#define HAVE_NETDB_H 1

/* Define to 1 if you have the <netinet/in.h> header file. */
#define HAVE_NETINET_IN_H 1

/* Define this if /opt/local/pgsql/include was detected */
/* #undef HAVE_OPT_LOCAL_PGSQL_INCLUDE */

/* Define this as PIC option */
/* #undef HAVE_PIC */

/* Define to 1 if you have the <plot.h> header file. */
/* #undef HAVE_PLOT_H */

/* Define to 1 if you have the `posix_memalign' function. */
#define HAVE_POSIX_MEMALIGN 1

/* Define this if a good PostgreSQL installation was detected */
/* #undef HAVE_POSTGRESQL */

/* Define to 1 if you have the `printf' function. */
#define HAVE_PRINTF 1

/* Define to 1 if you have the <pthread.h> header file. */
/* #undef HAVE_PTHREAD_H */

/* Define this if a good quadmath installation was detected */
/* #undef HAVE_QUADMATH */

/* Define to 1 if you have the <quadmath.h> header file. */
/* #undef HAVE_QUADMATH_H */

/* Define this if readline was detected */
/* #undef HAVE_READLINE */

/* Define to 1 if you have the <readline/history.h> header file. */
/* #undef HAVE_READLINE_HISTORY_H */

/* Define to 1 if you have the <readline/readline.h> header file. */
/* #undef HAVE_READLINE_READLINE_H */

/* Define to 1 if you have the <regex.h> header file. */
#define HAVE_REGEX_H 1

/* Define to 1 if you have the <Rmath.h> header file. */
/* #undef HAVE_RMATH_H */

/* Define to 1 if you have the `setjmp' function. */
#define HAVE_SETJMP 1

/* Define to 1 if you have the <setjmp.h> header file. */
#define HAVE_SETJMP_H 1

/* Define to 1 if you have the `signal' function. */
#define HAVE_SIGNAL 1

/* Define to 1 if you have the <signal.h> header file. */
#define HAVE_SIGNAL_H 1

/* Define to 1 if you have the `snprintf' function. */
#define HAVE_SNPRINTF 1

/* Define to 1 if you have the `sqrt' function. */
#define HAVE_SQRT 1

/* Define to 1 if you have the <stdarg.h> header file. */
#define HAVE_STDARG_H 1

/* Define to 1 if you have the <stddef.h> header file. */
#define HAVE_STDDEF_H 1

/* Define to 1 if you have the <stdint.h> header file. */
#define HAVE_STDINT_H 1

/* Define to 1 if you have the <stdio.h> header file. */
#define HAVE_STDIO_H 1

/* Define to 1 if you have the <stdlib.h> header file. */
#define HAVE_STDLIB_H 1

/* Define to 1 if you have the `strcmp' function. */
#define HAVE_STRCMP 1

/* Define to 1 if you have the <strings.h> header file. */
#define HAVE_STRINGS_H 1

/* Define to 1 if you have the <string.h> header file. */
#define HAVE_STRING_H 1

/* Define to 1 if you have the `strncmp' function. */
#define HAVE_STRNCMP 1

/* Define to 1 if you have the `strncpy' function. */
#define HAVE_STRNCPY 1

/* Define to 1 if you have the <sys/dir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_DIR_H */

/* Define to 1 if you have the <sys/ioctl.h> header file. */
#define HAVE_SYS_IOCTL_H 1

/* Define to 1 if you have the <sys/ndir.h> header file, and it defines `DIR'.
   */
/* #undef HAVE_SYS_NDIR_H */

/* Define to 1 if you have the <sys/resource.h> header file. */
#define HAVE_SYS_RESOURCE_H 1

/* Define to 1 if you have the <sys/socket.h> header file. */
#define HAVE_SYS_SOCKET_H 1

/* Define to 1 if you have the <sys/stat.h> header file. */
#define HAVE_SYS_STAT_H 1

/* Define to 1 if you have the <sys/time.h> header file. */
#define HAVE_SYS_TIME_H 1

/* Define to 1 if you have the <sys/types.h> header file. */
#define HAVE_SYS_TYPES_H 1

/* Define to 1 if you have <sys/wait.h> that is POSIX.1 compatible. */
#define HAVE_SYS_WAIT_H 1

/* Define to 1 if you have the <termios.h> header file. */
#define HAVE_TERMIOS_H 1

/* Define to 1 if you have the <time.h> header file. */
#define HAVE_TIME_H 1

/* Define this if user wants to tune for a specific CPU */
/* #undef HAVE_TUNING */

/* Define to 1 if you have the <unistd.h> header file. */
#define HAVE_UNISTD_H 1

/* Define this if untested OS was detected */
/* #undef HAVE_UNTESTED */

/* Define this if /usr/include/postgresql was detected */
/* #undef HAVE_USR_INCLUDE_POSTGRESQL */

/* Define this if /usr/local/pgsql/include was detected */
/* #undef HAVE_USR_LOCAL_PGSQL_INCLUDE */

/* Define this if /usr/pkg/pgsql/include was detected */
/* #undef HAVE_USR_PKG_PGSQL_INCLUDE */

/* Platform dependent */
#define INCLUDE_DIR ""

/* Define to 1 if assertions should be disabled. */
/* #undef NDEBUG */

/* Name of package */
#define PACKAGE "algol68g"

/* Define to the address where bug reports for this package should be sent. */
#define PACKAGE_BUGREPORT "Marcel van der Veer <algol68g@xs4all.nl>"

/* Define to the full name of this package. */
#define PACKAGE_NAME "algol68g"

/* Define to the full name and version of this package. */
#define PACKAGE_STRING "algol68g 3.1.9"

/* Define to the one symbol short name of this package. */
#define PACKAGE_TARNAME "algol68g"

/* Define to the home page for this package. */
#define PACKAGE_URL ""

/* Define to the version of this package. */
#define PACKAGE_VERSION "3.1.9"

/* Define to 1 if you have the ANSI C header files. */
#define STDC_HEADERS 1

/* Version number of package */
#define VERSION "3.1.9"

/* Define to 1 if type `char' is unsigned and you are not using gcc.  */
#ifndef __CHAR_UNSIGNED__
/* # undef __CHAR_UNSIGNED__ */
#endif

/* Define this if we have no __mode_t */
/* #undef __mode_t */

/* Define this if we have no __off_t */
/* #undef __off_t */

/* Define this if we have no __pid_t */
#define a68_pid_t pid_t

/* Define to `__inline__' or `__inline' if that's what the C compiler
   calls it, or to nothing if 'inline' is not supported under any name.  */
#ifndef __cplusplus
/* #undef inline */
#endif

/* Define to `int' if <sys/types.h> does not define. */
/* #undef mode_t */

/* Define to `unsigned int' if <sys/types.h> does not define. */
/* #undef size_t */

/* Define to `int' if <sys/types.h> does not define. */
/* #undef ssize_t */

/* Define to the type of an unsigned integer type of width exactly 16 bits if
   such a type exists and the standard includes do not define it. */
/* #undef uint16_t */
