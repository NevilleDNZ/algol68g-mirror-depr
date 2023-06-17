//! @file a68g-includes.h
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

#if !defined (__A68G_INCLUDES_H__)
#define __A68G_INCLUDES_H__

// Includes

#if defined (HAVE_SYS_TYPES_H)
#include <sys/types.h>
#endif

#if defined (HAVE_STDIO_H)
#include <stdio.h>
#endif

#if defined (HAVE_LIMITS_H)
#include <limits.h>
#endif

#if defined (HAVE_ASSERT_H)
#include <assert.h>
#endif

#if defined (HAVE_CONIO_H)
#include <conio.h>
#endif

#if defined (HAVE_CTYPE_H)
#include <ctype.h>
#endif

#if defined (HAVE_CURSES_H)
#include <curses.h>
#elif defined (HAVE_NCURSES_CURSES_H)
#include <ncurses/curses.h>
#endif

#if defined (HAVE_LIBGEN_H)
// libgen selects Posix versions of dirname/basename in stead of GNU versions.
#include <libgen.h>
#endif

#if defined (HAVE_READLINE_READLINE_H)
#include <readline/readline.h>
#endif

#if defined (HAVE_READLINE_HISTORY_H)
#include <readline/history.h>
#endif

#if defined (HAVE_DIRENT_H)
#include <dirent.h>
#endif

#if defined (HAVE_DL)
#include <dlfcn.h>
#endif

#if defined (HAVE_ERRNO_H)
#include <errno.h>
#endif

#if defined (HAVE_FCNTL_H)
#include <fcntl.h>
#endif

#if defined (HAVE_FENV_H)
#include <fenv.h>
#endif

#if defined (HAVE_FLOAT_H)
#include <float.h>
#endif

#if defined (HAVE_LIBPQ_FE_H)
# if ! defined (A68_OPTIMISE)
#  include <libpq-fe.h>
# endif
#endif

#if defined (HAVE_MATH_H)
#include <math.h>
#endif

#if defined (HAVE_COMPLEX_H)
#include <complex.h>
#undef I
#endif

#if defined (HAVE_NETDB_H)
#include <netdb.h>
#endif

#if defined (HAVE_NETINET_IN_H)
#include <netinet/in.h>
#endif

#if defined (HAVE_GNU_PLOTUTILS)
#include <plot.h>
#endif

#if defined (HAVE_PTHREAD_H)
#include <pthread.h>
#endif

#if defined (HAVE_QUADMATH_H)
#include <quadmath.h>
#endif

#if defined (HAVE_SETJMP_H)
#include <setjmp.h>
#endif

#if defined (HAVE_SIGNAL_H)
#include <signal.h>
#endif

#if defined (HAVE_STDARG_H)
#include <stdarg.h>
#endif

#if defined (HAVE_STDBOOL_H)
#include <stdbool.h>
#endif

#if defined (HAVE_STDDEF_H)
#include <stddef.h>
#endif

#if defined (HAVE_STDINT_H)
#include <stdint.h>
#endif

#if defined (HAVE_STDLIB_H)
#include <stdlib.h>
#endif

#if defined (HAVE_STRING_H)
#include <string.h>
#endif

#if defined (HAVE_STRINGS_H)
#include <strings.h>
#endif

#if (defined (HAVE_TERMIOS_H) && ! defined (TIOCGWINSZ))
#include <termios.h>
#elif (defined (HAVE_TERMIOS_H) && ! defined (GWINSZ_IN_SYS_IOCTL))
#include <termios.h>
#endif

#if defined (HAVE_TIME_H)
#include <time.h>
#endif

#if defined (HAVE_UNISTD_H)
#include <unistd.h>
#endif

#if defined (HAVE_SYS_IOCTL_H)
#include <sys/ioctl.h>
#endif

#if defined (HAVE_SYS_RESOURCE_H)
#include <sys/resource.h>
#endif

#if defined (HAVE_SYS_SOCKET_H)
#include <sys/socket.h>
#endif

#if defined (HAVE_SYS_STAT_H)
#include <sys/stat.h>
#endif

#if defined (HAVE_SYS_TIME_H)
#include <sys/time.h>
#endif

#if defined (HAVE_SYS_WAIT_H)
#include <sys/wait.h>
#endif

#if defined (HAVE_GNU_MPFR)
#define MPFR_WANT_FLOAT128
#include <gmp.h>
#include <mpfr.h>
#endif

#if defined (HAVE_GSL_GSL_BLAS_H)
#include <gsl/gsl_blas.h>
#endif

#if defined (HAVE_GSL_GSL_COMPLEX_H)
#include <gsl/gsl_complex.h>
#endif

#if defined (HAVE_GSL_GSL_COMPLEX_MATH_H)
#include <gsl/gsl_complex_math.h>
#endif

#if defined (HAVE_GSL_GSL_EIGEN_H)
#include <gsl/gsl_eigen.h>
#endif

#if defined (HAVE_GSL_GSL_ERRNO_H)
#include <gsl/gsl_errno.h>
#endif

#if defined (HAVE_GSL_GSL_FFT_COMPLEX_H)
#include <gsl/gsl_fft_complex.h>
#endif

#if defined (HAVE_GSL_GSL_INTEGRATION_H)
#include <gsl/gsl_integration.h>
#endif

#if defined (HAVE_GSL_GSL_SF_H)
#include <gsl/gsl_sf.h>
#endif

#if defined (HAVE_GSL_GSL_SF_ELLINT_H)
#include <gsl/gsl_sf_ellint.h>
#endif

#if defined (HAVE_GSL_GSL_SF_HERMITE_H)
#include <gsl/gsl_sf_hermite.h>
#endif

#if defined (HAVE_GSL_GSL_LINALG_H)
#include <gsl/gsl_linalg.h>
#endif

#if defined (HAVE_GSL_GSL_MATH_H)
#include <gsl/gsl_math.h>
#endif

#if defined (HAVE_GSL_GSL_MATRIX_H)
#include <gsl/gsl_matrix.h>
#endif

#if defined (HAVE_GSL_GSL_PERMUTATION_H)
#include <gsl/gsl_permutation.h>
#endif

#if defined (HAVE_GSL_GSL_SF_H)
#include <gsl/gsl_sf.h>
#endif

#if defined (HAVE_GSL_GSL_STATISTICS_H)
#include <gsl/gsl_statistics.h>
#endif

#if defined (HAVE_GSL_GSL_VECTOR_H)
#include <gsl/gsl_vector.h>
#endif

#if defined (HAVE_GSL_GSL_VERSION_H)
#include <gsl/gsl_version.h>
#endif

#if defined (HAVE_REGEX_H)
# if defined (BUILD_WIN32)
#  include "a68g-regex.h"
# else
#  include <regex.h>
# endif
#endif

#endif
