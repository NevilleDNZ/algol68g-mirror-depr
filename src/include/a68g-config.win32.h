//! @file a68g-config.win32.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2023 J. Marcel van der Veer <algol68g@xs4all.nl>.
//
//! @section License
//
// This program is free software; you can redistribute it and/or modify it 
// under the terms of the GNU General Public License as published by the 
// Free Software Foundation; either version 3 of the License, or 
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful, but 
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
// more details. You should have received a copy of the GNU General Public 
// License along with this program. If not, see <http://www.gnu.org/licenses/>.

//! @section Synopsis
//!
//! Default WIN32 configuration file.

#if defined (BUILD_WIN32)

#define HAVE_ISINF 1
#define HAVE_ISNAN 1
#define HAVE_ISFINITE 1

#define HAVE_GSL
#define HAVE_GNU_PLOTUTILS
#define HAVE_MATHLIB
#define HAVE_QUADMATH

#undef HAVE_GNU_MPFR
#undef HAVE_CURSES

#define HAVE_QUADMATH_H
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
#undef HAVE_PTHREAD_H
#undef HAVE_TERM_H
#define HAVE_STDINT_H

#define HAVE_STDARG_H
#define HAVE_STDLIB_H
#define HAVE_ERRNO_H
#define HAVE_ASSERT_H
#define HAVE_CONIO_H
#define HAVE_CTYPE_H
#define HAVE_DIRENT_H
#define HAVE_FCNTL_H
#define HAVE_FLOAT_H
#define HAVE_LIBGEN_H
#define HAVE_LIMITS_H
#define HAVE_MATH_H
#define HAVE_COMPLEX_H
#define HAVE_SETJMP_H
#define HAVE_SIGNAL_H
#define HAVE_STDIO_H
#define HAVE_STRING_H
#define HAVE_SYS_STAT_H
#define HAVE_SYS_TYPES_H
#define HAVE_TIME_H
#define HAVE_UNISTD_H
#define HAVE_WINSOCK_H

#endif // BUILD_WIN32

// Name of package
#define PACKAGE "algol68g"

// Define to the full name of this package.
#define PACKAGE_NAME "algol68g"

// Define to the one symbol short name of this package.
#define PACKAGE_TARNAME "algol68g"

// Define to the address where bug reports for this package should be sent.
#define PACKAGE_BUGREPORT "Marcel van der Veer <algol68g@xs4all.nl>"

// Define to the full name and version of this package.
#define PACKAGE_STRING "algol68g 3.2.0"

// Define to the version of this package.
#define PACKAGE_VERSION "3.2.0"

// Version number of package
#define VERSION "3.2.0"

