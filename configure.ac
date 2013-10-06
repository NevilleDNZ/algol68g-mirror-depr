AC_INIT([algol68g], [2.8], [Marcel van der Veer <algol68g@xs4all.nl>])
#
# Algol 68 Genie "configure.ac" from "a68g-tools".
#

AC_PREREQ([2.60])

# Check whether compiler supports $1 as a command-line option. 
# If it does, add the string to CFLAGS.

AC_DEFUN([A68G_AC_PROG_CC_CFLAGS], 
  [AC_MSG_CHECKING([whether $CC accepts $1])
  a68g_ac_save_CFLAGS=$CFLAGS
  CFLAGS="$a68g_ac_save_CFLAGS $1"
  _AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], 
    AC_MSG_RESULT(yes), 
    [CFLAGS="$a68g_ac_save_CFLAGS"
    AC_MSG_RESULT(no)])
])

# Check whether $1 is in GNU plotutils.

AC_DEFUN([A68G_AC_PLOTUTILS],
  [AC_CHECK_DECL([$1], [], [enable_plotutils=no], [
#include <stdio.h> 
#include <plot.h>
])
  AC_CHECK_LIB([plot], [$1], [a68g_unexpected=yes], [enable_plotutils=no])
])

# Check whether $1 is in GNU GSL.

AC_DEFUN([A68G_AC_GSL],
  [AC_CHECK_DECL([$1], [], [enable_gsl=no], [
#include <gsl/gsl_blas.h> 
#include <gsl/gsl_complex.h> 
#include <gsl/gsl_complex.h> 
#include <gsl/gsl_complex.h> 
#include <gsl/gsl_complex_math.h> 
#include <gsl/gsl_complex_math.h> 
#include <gsl/gsl_complex_math.h> 
#include <gsl/gsl_errno.h> 
#include <gsl/gsl_errno.h> 
#include <gsl/gsl_errno.h> 
#include <gsl/gsl_fft_complex.h> 
#include <gsl/gsl_integration.h> 
#include <gsl/gsl_linalg.h> 
#include <gsl/gsl_math.h> 
#include <gsl/gsl_math.h> 
#include <gsl/gsl_math.h> 
#include <gsl/gsl_matrix.h> 
#include <gsl/gsl_permutation.h> 
#include <gsl/gsl_sf.h> 
#include <gsl/gsl_sf.h> 
#include <gsl/gsl_sf.h> 
#include <gsl/gsl_vector.h> 
])
  AC_CHECK_LIB([gsl], [$1], [a68g_unexpected=yes], [enable_gsl=no])
])

# Check whether $1 is in PostgreSQL.

AC_DEFUN([A68G_AC_POSTGRESQL],
  [AC_CHECK_DECL([$1], [], [enable_postgresql=no], [
#include <libpq-fe.h> 
])
  AC_CHECK_LIB([pq], [$1], [a68g_unexpected=yes], [enable_postgresql=no])
  ])

# Check whether $1 is in pthread.

AC_DEFUN([A68G_AC_PTHREAD],
  [AC_CHECK_DECL([$1], [], [enable_parallel=no], [
#include <pthread.h> 
])
  AC_CHECK_LIB([pthread], [$1], [a68g_unexpected=yes], [enable_parallel=no])
  ])

# Check whether $1 is in dl.

AC_DEFUN([A68G_AC_DL],
  [AC_CHECK_DECL([$1], [], [enable_compiler=no], [
#include <dlfcn.h> 
])
  AC_CHECK_LIB([dl], [$1], [a68g_unexpected=yes], [enable_compiler=no])
  ])

#
# Platform ids.
#

AC_MSG_NOTICE([host system...])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

AC_MSG_CHECKING([platform])
case "$host" in
#
# Linux.
#
*86-*-gnu | *86_64-*-gnu | *86-*-linux* | *86_64-*-linux* | arm*-*-linux*) 
  AC_DEFINE(HAVE_LINUX, 1, [Define this if LINUX was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant])
  AC_MSG_RESULT([linux])
  ;;
#
# Cygwin.
#
*86-*-cygwin* | *86_64-*-cygwin*) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant])
  AC_MSG_RESULT([cygwin])
  ;;
#
# Mac OS X.
#
*86-*-*darwin* | *86_64-*-*darwin*) 
  AC_DEFINE(HAVE_MAC_OS_X, 1, [Define this if DARWIN was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_RESULT([mac os x])
  ;;
#
# FreeBSD.
#
*86-*-freebsd* | *86_64-*-freebsd*) 
  AC_DEFINE(HAVE_FREEBSD, 1, [Define this if FreeBSD was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_RESULT([freebsd])
  ;;
#
# NetBSD.
#
*86-*-netbsd* | *86_64-*-netbsd*) 
  AC_DEFINE(HAVE_NETBSD, 1, [Define this if NetBSD was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_RESULT([netbsd])
  ;;
#
# OpenBSD.
#
*86-*-openbsd* | *86_64-*-openbsd*) 
  AC_DEFINE(HAVE_OPENBSD, 1, [Define this if OpenBSD was detected]) 
  AC_MSG_WARN([configuring interpreter-only on OpenBSD])
  AC_MSG_RESULT([openbsd])
  enable_compiler=no
  ;;
#
# Others, untested.
#
*)
  AC_DEFINE(HAVE_UNTESTED, 1, [Define this if untested OS was detected]) 
  AC_MSG_WARN([configuring interpreter-only on untested platform])
  AC_MSG_RESULT([interpreter-only])
  enable_compiler=no
  ;;
esac

#
# Extra options.
#

AC_ARG_ENABLE(arch, [AS_HELP_STRING([--enable-arch=cpu], [if using gcc, enable emitting architecture-tuned assembly code (default is "no")])], , enable_arch=no) 
AC_ARG_ENABLE(compiler, [AS_HELP_STRING([--enable-compiler], [enable unit compiler (default is "yes")])], , enable_compiler=yes) 
AC_ARG_ENABLE(curses, [AS_HELP_STRING([--enable-curses], [if installed, enable curses library (default is "yes")])], , enable_curses=yes) 
AC_ARG_ENABLE(readline, [AS_HELP_STRING([--enable-readline], [if installed, enable readline library (default is "yes")])], , enable_readline=yes) 
AC_ARG_ENABLE(gsl, [AS_HELP_STRING([--enable-gsl], [if installed, enable GNU Scientific Library (default is "yes")])], , enable_gsl=yes) 
AC_ARG_ENABLE(parallel, [AS_HELP_STRING([--enable-parallel], [enable Algol 68 parallel-clause (default is "yes")])], , enable_parallel=yes) 
AC_ARG_ENABLE(pic, [AS_HELP_STRING([--enable-pic=option], [if using gcc, enable option to generate PIC (default is "-fPIC")])], , enable_pic="-fPIC") 
AC_ARG_ENABLE(prescott, [AS_HELP_STRING([--enable-prescott], [if using gcc, enable optimisation for P4 Prescott (default is "no")])], , enable_prescott=no) 
AC_ARG_ENABLE(plotutils, [AS_HELP_STRING([--enable-plotutils], [if installed, enable GNU plotting utilities (default is "yes")])], , enable_plotutils=yes) 
AC_ARG_ENABLE(postgresql, [AS_HELP_STRING([--enable-postgresql], [if installed, enable PostgreSQL (default is "yes")])], , enable_postgresql=yes) 

#
# Initialisation.
#

AC_MSG_NOTICE([initialising...])
AM_INIT_AUTOMAKE([subdir-objects])
AC_PREFIX_DEFAULT(/usr/local)
AC_CONFIG_SRCDIR([source/a68g.h])
AC_CONFIG_HEADERS([source/a68g-config.h])
AC_PROG_INSTALL
AC_CHECK_PROG(a68g_exists, a68g, "yes")

#
# C compiler.
#

AC_MSG_NOTICE([C compiler...])
AC_LANG(C)
AC_PROG_CC([clang gcc cc])
if test "x$GCC" != "xyes"; then
  a68g_ac_compiler=no
  AC_MSG_WARN([gcc is the preferred C compiler; configuring interpreter-only])
else
  AC_DEFINE(HAVE_GCC, 1, [Define this if GCC was detected]) 

  if test "x$enable_prescott" != "xno"; then
    CFLAGS="-O3 -fomit-frame-pointer -march=prescott -funroll-loops"
    enable_pic="no"
  fi

  A68G_AC_PROG_CC_CFLAGS([-pedantic])
  A68G_AC_PROG_CC_CFLAGS([-W])
  A68G_AC_PROG_CC_CFLAGS([-Wall])
  A68G_AC_PROG_CC_CFLAGS([-Wextra])
  A68G_AC_PROG_CC_CFLAGS([-Wshadow])
# A68G_AC_PROG_CC_CFLAGS([-Wconversion]) Too much warnings!
  A68G_AC_PROG_CC_CFLAGS([-Wstrict-prototypes])
  A68G_AC_PROG_CC_CFLAGS([-Wchar-subscripts])

#
# Test on gcc capabilities.
#
#
# Check -Wl,--export-dynamic, needed for creating shared objects.
#
# Check whether we can link to a particular function, not just whether we can link.
# In fact, we must actually check that the resulting program runs.
#
    a68g_ac_arg="-Wl,--export-dynamic"
    AC_MSG_CHECKING([if $CC accepts $a68g_ac_arg])
    a68g_ac_save_LDFLAGS=$LDFLAGS
    LDFLAGS="$a68g_ac_save_LDFLAGS $a68g_ac_arg"
    AC_RUN_IFELSE([AC_LANG_PROGRAM([extern void exit (); void (*fptr) () = exit;],[])],
      [AC_MSG_RESULT(yes)
       AC_DEFINE(HAVE_EXPORT_DYNAMIC, 1, [Define this if EXPORT_DYNAMIC is recognised]) 
      ],
      [AC_MSG_RESULT(no)
       AC_MSG_WARN([--export-dynamic is not accepted; configuring interpreter-only])
       a68g_ac_compiler=no
       LDFLAGS=$a68g_ac_save_LDFLAGS
      ],
      [AC_MSG_RESULT(assuming no)
       AC_MSG_WARN([--export-dynamic is not accepted; configuring interpreter-only])
       a68g_ac_compiler=no
       LDFLAGS=$a68g_ac_save_LDFLAGS
      ]
    )
  fi
  AM_CONDITIONAL([EXPORT_DYNAMIC], [test "x$a68g_ac_compiler" = "xyes"])
#
# Optionally, tune for a specific processor.
#
  if test "x$enable_arch" != "xno"; then
    AC_MSG_CHECKING([whether $CC accepts -march=$enable_arch])
    a68g_ac_save_CFLAGS=$CFLAGS
    a68g_ac_march="-march=$enable_arch"
    CFLAGS="$a68g_ac_save_CFLAGS $a68g_ac_march"
    _AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], 
      [AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_TUNING, "$a68g_ac_march", [Define this if user wants to tune for a specific CPU]) 
      ], 
      [AC_MSG_RESULT(no)
       AC_MSG_WARN([your CPU name is not accepted; resetting to default])
       CFLAGS="$a68g_ac_save_CFLAGS"
      ]
    )
  fi
#
# Some platforms want another or no PIC option.
#
if test "x$enable_compiler" = "xyes"; then
  if test "x$enable_pic" != "xno"; then
    AC_MSG_CHECKING([whether $CC accepts $enable_pic])
    a68g_ac_save_CFLAGS=$CFLAGS
    CFLAGS="$a68g_ac_save_CFLAGS $enable_pic"
    _AC_COMPILE_IFELSE([AC_LANG_PROGRAM()], 
      [AC_MSG_RESULT(yes)
       AC_DEFINE_UNQUOTED(HAVE_PIC, "$enable_pic", [Define this as PIC option]) 
      ], 
      [AC_MSG_RESULT(no)
       AC_MSG_WARN([your PIC option is not accepted; configuring interpreter-only])
      ]
    )
    CFLAGS="$a68g_ac_save_CFLAGS"
  fi
fi

AM_PROG_CC_C_O
AC_PROG_CPP
AC_MSG_NOTICE([types...])
AC_C_CHAR_UNSIGNED
AC_TYPE_MODE_T
AC_TYPE_SIZE_T
AC_TYPE_SSIZE_T
AC_TYPE_UINT16_T

AC_MSG_CHECKING([__off_t or off_t])
AC_COMPILE_IFELSE(
[[
#include <unistd.h>
#include <sys/types.h>
__off_t dummy;
]],
  [AC_MSG_RESULT([__off_t])],
  [AC_MSG_RESULT([off_t])
   AC_DEFINE(__off_t, off_t, [Define this if we have no __off_t])]
)

AC_MSG_CHECKING([__pid_t or pid_t])
AC_COMPILE_IFELSE(
[[
#include <unistd.h>
#include <sys/types.h>
__pid_t dummy;
]],
  [AC_MSG_RESULT([__pid_t])],
  [AC_MSG_RESULT([pid_t])
   AC_DEFINE(__pid_t, pid_t, [Define this if we have no __pid_t])]
)

AC_MSG_CHECKING([__mode_t or mode_t])
AC_COMPILE_IFELSE(
[[
#include <unistd.h>
#include <sys/types.h>
__mode_t dummy;
]],
  [AC_MSG_RESULT([__mode_t])],
  [AC_MSG_RESULT([mode_t])
   AC_DEFINE(__mode_t, mode_t, [Define this if we have no __mode_t])]
)

#
# Extra include directories.
#

AC_MSG_NOTICE([extra include directories...])
if test -d /usr/local/pgsql/include; then
  AC_DEFINE(HAVE_USR_LOCAL_PGSQL_INCLUDE, 1, [Define this if /usr/local/pgsql/include was detected]) 
  CFLAGS="$CFLAGS -I/usr/local/pgsql/include"
  CPPFLAGS="$CPPFLAGS -I/usr/local/pgsql/include"
  CXXFLAGS="$CXXFLAGS -I/usr/local/pgsql/include"
  LDFLAGS="$LDFLAGS -L/usr/local/pgsql/lib"
fi

if test -d /usr/pkg/pgsql/include; then
  AC_DEFINE(HAVE_USR_PKG_PGSQL_INCLUDE, 1, [Define this if /usr/pkg/pgsql/include was detected]) 
  CFLAGS="$CFLAGS -I/usr/pkg/pgsql/include"
  CPPFLAGS="$CPPFLAGS -I/usr/pkg/pgsql/include"
  CXXFLAGS="$CXXFLAGS -I/usr/pkg/pgsql/include"
  LDFLAGS="$LDFLAGS -L/usr/pkg/pgsql/lib"
fi

if test -d /opt/local/pgsql/include; then
  AC_DEFINE(HAVE_OPT_LOCAL_PGSQL_INCLUDE, 1, [Define this if /opt/local/pgsql/include was detected]) 
  CFLAGS="$CFLAGS -I/opt/local/pgsql/include"
  CPPFLAGS="$CPPFLAGS -I/opt/local/pgsql/include"
  CXXFLAGS="$CXXFLAGS -I/opt/local/pgsql/include"
  LDFLAGS="$LDFLAGS -L/opt/local/pgsql/lib"
fi

#
# Checks for header files.
#

AC_MSG_NOTICE([standard header files...])
#
# <math.h> test is GSL proof.
#

AC_CHECK_HEADERS([math.h])
AC_CHECK_LIB([m], [cos])

AC_HEADER_STDC
AC_HEADER_ASSERT
AC_HEADER_DIRENT
AC_HEADER_SYS_WAIT
AC_HEADER_TIOCGWINSZ

AC_CHECK_HEADERS([assert.h ctype.h errno.h fcntl.h float.h limits.h netdb.h netinet/in.h regex.h setjmp.h signal.h stdarg.h stddef.h stdio.h sys/ioctl.h sys/resource.h sys/socket.h sys/time.h termios.h time.h])

#
# Functions we expect
#

AC_MSG_NOTICE([standard functions...])
AC_CHECK_FUNCS(exit)
AC_CHECK_FUNCS(fprintf)
AC_CHECK_FUNCS(free)
AC_CHECK_FUNCS(longjmp)
AC_CHECK_FUNCS(malloc)
AC_CHECK_FUNCS(memcpy)
AC_CHECK_FUNCS(memmove)
AC_CHECK_FUNCS(memset)
AC_CHECK_FUNCS(printf)
AC_CHECK_FUNCS(setjmp)
AC_CHECK_FUNCS(signal)
AC_CHECK_FUNCS(snprintf)
AC_CHECK_FUNCS(strcmp)
AC_CHECK_FUNCS(strncmp)
AC_CHECK_FUNCS(strncpy)

AC_MSG_NOTICE([optional headers and libraries...])

if test "x$enable_curses" = "xyes"; then
  AC_CHECK_HEADERS([curses.h], [], [enable_curses=no], [])
  if test "x$enable_curses" = "xno"; then
    AC_CHECK_HEADERS([ncurses/curses.h], [enable_curses=yes], [], [])
  fi
  if test "x$enable_curses" = "xyes"; then
    AC_CHECK_LIB([ncurses], [initscr], [], [enable_curses=no])
    if test "x$enable_curses" = "xyes"; then
      if test "x$enable_readline" = "xyes"; then
        AC_CHECK_HEADERS([readline/readline.h], [], [enable_readline=no], [])
        AC_CHECK_HEADERS([readline/history.h], [], [enable_readline=no], [])
        if test "x$enable_readline" = "xyes"; then
          AC_CHECK_LIB([readline], [readline], [], [enable_readline=no], [-lcurses])
          if test "x$enable_readline" = "xyes"; then
            AC_DEFINE(HAVE_READLINE, 1, [Define this if readline was detected]) 
          fi
        fi
      fi
    fi
    if test "x$enable_curses" = "xno"; then
      AC_CHECK_LIB([curses], [initscr], [enable_curses=yes], [])
    fi
    if test "x$enable_curses" = "xyes"; then
      AC_DEFINE(HAVE_CURSES, 1, [Define this if curses was detected]) 
    fi
  fi
fi

if test "x$enable_plotutils" = "xyes"; then
  AC_MSG_NOTICE([GNU plotutils...])
  AC_CHECK_HEADERS([plot.h], [], [enable_plotutils=no], [])
  A68G_AC_PLOTUTILS(pl_alabel_r)
  if test "x$enable_plotutils" = "xyes"; then
    A68G_AC_PLOTUTILS(pl_bgcolor_r)
    A68G_AC_PLOTUTILS(pl_bgcolorname_r)
    A68G_AC_PLOTUTILS(pl_color_r)
    A68G_AC_PLOTUTILS(pl_colorname_r)
    A68G_AC_PLOTUTILS(pl_erase_r)
    A68G_AC_PLOTUTILS(pl_fbox_r)
    A68G_AC_PLOTUTILS(pl_fcircle_r)
    A68G_AC_PLOTUTILS(pl_fillcolor_r)
    A68G_AC_PLOTUTILS(pl_fillcolorname_r)
    A68G_AC_PLOTUTILS(pl_filltype_r)
    A68G_AC_PLOTUTILS(pl_fline_r)
    A68G_AC_PLOTUTILS(pl_fmove_r)
    A68G_AC_PLOTUTILS(pl_fontname_r)
    A68G_AC_PLOTUTILS(pl_fontsize_r)
    A68G_AC_PLOTUTILS(pl_fpoint_r)
    A68G_AC_PLOTUTILS(pl_linemod_r)
    A68G_AC_PLOTUTILS(pl_linewidth_r)
    A68G_AC_PLOTUTILS(pl_newplparams)
    A68G_AC_PLOTUTILS(pl_openpl_r)
    A68G_AC_PLOTUTILS(pl_pencolor_r)
    A68G_AC_PLOTUTILS(pl_pencolorname_r)
    A68G_AC_PLOTUTILS(pl_setplparam)
    A68G_AC_PLOTUTILS(pl_space_r)
    A68G_AC_PLOTUTILS(pl_textangle_r)
  fi
  if test "x$enable_plotutils" = "xyes"; then
    AC_CHECK_LIB([plot], [pl_alabel_r])
    AC_DEFINE(HAVE_GNU_PLOTUTILS, 1, [Define this if a good GNU plotutils installation was detected]) 
  fi
fi

if test "x$enable_gsl" = "xyes"; then
  AC_MSG_NOTICE([GNU scientific library...])
  AC_CHECK_HEADERS([gsl/gsl_blas.h gsl/gsl_complex.h gsl/gsl_complex_math.h gsl/gsl_errno.h gsl/gsl_fft_complex.h gsl/gsl_integration.h gsl/gsl_linalg.h gsl/gsl_math.h gsl/gsl_matrix.h gsl/gsl_permutation.h gsl/gsl_sf.h gsl/gsl_vector.h], [], [enable_gsl=no], [])
  AC_CHECK_LIB([gslcblas], [cblas_dgemm], [], [enable_gsl=no])
  A68G_AC_GSL(gsl_blas_ddot)
  if test "x$enable_gsl" = "xyes"; then
    A68G_AC_GSL(gsl_blas_dgemm)
    A68G_AC_GSL(gsl_blas_dgemv)
    A68G_AC_GSL(gsl_blas_zaxpy)
    A68G_AC_GSL(gsl_blas_zdotc)
    A68G_AC_GSL(gsl_blas_zdscal)
    A68G_AC_GSL(gsl_blas_zgemm)
    A68G_AC_GSL(gsl_blas_zgemv)
    A68G_AC_GSL(gsl_blas_zscal)
    A68G_AC_GSL(gsl_complex_arccosh)
    A68G_AC_GSL(gsl_complex_arcsinh)
    A68G_AC_GSL(gsl_complex_arctanh)
    A68G_AC_GSL(gsl_complex_cosh)
    A68G_AC_GSL(gsl_complex_inverse)
    A68G_AC_GSL(gsl_complex_mul)
    A68G_AC_GSL(gsl_complex_sinh)
    A68G_AC_GSL(gsl_complex_tanh)
    A68G_AC_GSL(gsl_fft_complex_backward)
    A68G_AC_GSL(gsl_fft_complex_forward)
    A68G_AC_GSL(gsl_fft_complex_inverse)
    A68G_AC_GSL(gsl_fft_complex_wavetable_alloc)
    A68G_AC_GSL(gsl_fft_complex_wavetable_free)
    A68G_AC_GSL(gsl_fft_complex_workspace_alloc)
    A68G_AC_GSL(gsl_fft_complex_workspace_free)
    A68G_AC_GSL(gsl_integration_qagiu)
    A68G_AC_GSL(gsl_integration_workspace_alloc)
    A68G_AC_GSL(gsl_integration_workspace_free)
    A68G_AC_GSL(gsl_linalg_LU_decomp)
    A68G_AC_GSL(gsl_linalg_LU_det)
    A68G_AC_GSL(gsl_linalg_LU_invert)
    A68G_AC_GSL(gsl_linalg_LU_refine)
    A68G_AC_GSL(gsl_linalg_LU_solve)
    A68G_AC_GSL(gsl_linalg_QR_decomp)
    A68G_AC_GSL(gsl_linalg_QR_lssolve)
    A68G_AC_GSL(gsl_linalg_QR_solve)
    A68G_AC_GSL(gsl_linalg_SV_decomp)
    A68G_AC_GSL(gsl_linalg_SV_solve)
    A68G_AC_GSL(gsl_linalg_cholesky_decomp)
    A68G_AC_GSL(gsl_linalg_cholesky_solve)
    A68G_AC_GSL(gsl_linalg_complex_LU_decomp)
    A68G_AC_GSL(gsl_linalg_complex_LU_det)
    A68G_AC_GSL(gsl_linalg_complex_LU_invert)
    A68G_AC_GSL(gsl_linalg_complex_LU_refine)
    A68G_AC_GSL(gsl_linalg_complex_LU_solve)
    A68G_AC_GSL(gsl_matrix_add)
    A68G_AC_GSL(gsl_matrix_alloc)
    A68G_AC_GSL(gsl_matrix_complex_add)
    A68G_AC_GSL(gsl_matrix_complex_alloc)
    A68G_AC_GSL(gsl_matrix_complex_free)
    A68G_AC_GSL(gsl_matrix_complex_get)
    A68G_AC_GSL(gsl_matrix_complex_isnull)
    A68G_AC_GSL(gsl_matrix_complex_scale)
    A68G_AC_GSL(gsl_matrix_complex_set)
    A68G_AC_GSL(gsl_matrix_complex_set_zero)
    A68G_AC_GSL(gsl_matrix_complex_sub)
    A68G_AC_GSL(gsl_matrix_complex_transpose)
    A68G_AC_GSL(gsl_matrix_free)
    A68G_AC_GSL(gsl_matrix_get)
    A68G_AC_GSL(gsl_matrix_isnull)
    A68G_AC_GSL(gsl_matrix_scale)
    A68G_AC_GSL(gsl_matrix_set)
    A68G_AC_GSL(gsl_matrix_set_zero)
    A68G_AC_GSL(gsl_matrix_sub)
    A68G_AC_GSL(gsl_matrix_transpose)
    A68G_AC_GSL(gsl_permutation_alloc)
    A68G_AC_GSL(gsl_permutation_free)
    A68G_AC_GSL(gsl_permutation_get)
    A68G_AC_GSL(gsl_set_error_handler)
    A68G_AC_GSL(gsl_set_error_handler_off)
    A68G_AC_GSL(gsl_sf_airy_Ai_deriv_e)
    A68G_AC_GSL(gsl_sf_airy_Ai_e)
    A68G_AC_GSL(gsl_sf_airy_Bi_deriv_e)
    A68G_AC_GSL(gsl_sf_airy_Bi_e)
    A68G_AC_GSL(gsl_sf_bessel_In_e)
    A68G_AC_GSL(gsl_sf_bessel_In_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_Inu_e)
    A68G_AC_GSL(gsl_sf_bessel_Inu_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_Jn_e)
    A68G_AC_GSL(gsl_sf_bessel_Jnu_e)
    A68G_AC_GSL(gsl_sf_bessel_Kn_e)
    A68G_AC_GSL(gsl_sf_bessel_Kn_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_Knu_e)
    A68G_AC_GSL(gsl_sf_bessel_Knu_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_Yn_e)
    A68G_AC_GSL(gsl_sf_bessel_Ynu_e)
    A68G_AC_GSL(gsl_sf_bessel_il_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_jl_e)
    A68G_AC_GSL(gsl_sf_bessel_kl_scaled_e)
    A68G_AC_GSL(gsl_sf_bessel_yl_e)
    A68G_AC_GSL(gsl_sf_beta_e)
    A68G_AC_GSL(gsl_sf_beta_inc_e)
    A68G_AC_GSL(gsl_sf_ellint_Ecomp_e)
    A68G_AC_GSL(gsl_sf_ellint_Kcomp_e)
    A68G_AC_GSL(gsl_sf_ellint_RC_e)
    A68G_AC_GSL(gsl_sf_ellint_RD_e)
    A68G_AC_GSL(gsl_sf_ellint_RF_e)
    A68G_AC_GSL(gsl_sf_ellint_RJ_e)
    A68G_AC_GSL(gsl_sf_erf_e)
    A68G_AC_GSL(gsl_sf_erfc_e)
    A68G_AC_GSL(gsl_sf_fact)
    A68G_AC_GSL(gsl_sf_gamma_e)
    A68G_AC_GSL(gsl_sf_gamma_inc_P_e)
    A68G_AC_GSL(gsl_sf_lngamma_e)
    A68G_AC_GSL(gsl_strerror)
    A68G_AC_GSL(gsl_vector_add)
    A68G_AC_GSL(gsl_vector_alloc)
    A68G_AC_GSL(gsl_vector_complex_alloc)
    A68G_AC_GSL(gsl_vector_complex_free)
    A68G_AC_GSL(gsl_vector_complex_get)
    A68G_AC_GSL(gsl_vector_complex_isnull)
    A68G_AC_GSL(gsl_vector_complex_set)
    A68G_AC_GSL(gsl_vector_complex_set_zero)
    A68G_AC_GSL(gsl_vector_free)
    A68G_AC_GSL(gsl_vector_get)
    A68G_AC_GSL(gsl_vector_isnull)
    A68G_AC_GSL(gsl_vector_scale)
    A68G_AC_GSL(gsl_vector_set)
    A68G_AC_GSL(gsl_vector_set_zero)
    A68G_AC_GSL(gsl_vector_sub)
  fi
  if test "x$enable_gsl" = "xyes"; then
    AC_CHECK_LIB([gsl], [gsl_blas_ddot])
    AC_DEFINE(HAVE_GNU_GSL, 1, [Define this if a good GNU GSL installation was detected]) 
  fi
fi

if test "x$enable_parallel" = "xyes"; then
  AC_MSG_NOTICE([POSIX pthreads...])
  AC_CHECK_HEADERS([pthread.h], [], [enable_parallel=no], [])
  A68G_AC_PTHREAD(pthread_attr_getstacksize)
  if test "x$enable_parallel" = "xyes"; then
    A68G_AC_PTHREAD(pthread_attr_init)
    A68G_AC_PTHREAD(pthread_attr_setstacksize)
    A68G_AC_PTHREAD(pthread_create)
    A68G_AC_PTHREAD(pthread_equal)
    A68G_AC_PTHREAD(pthread_exit)
    A68G_AC_PTHREAD(pthread_join)
    A68G_AC_PTHREAD(pthread_mutex_lock)
    A68G_AC_PTHREAD(pthread_mutex_unlock)
    A68G_AC_PTHREAD(pthread_self)
  fi
  if test "x$enable_parallel" = "xyes"; then
    AC_CHECK_LIB([pthread], pthread_attr_getstacksize)
    AC_DEFINE(HAVE_PARALLEL_CLAUSE, 1, [Define this if a good pthread installation was detected]) 
  fi
fi

if test "x$enable_postgresql" = "xyes"; then
  AC_MSG_NOTICE([PostgreSQL...])
  AC_CHECK_HEADERS([libpq-fe.h], [], [enable_postgresql=no], [])
  A68G_AC_POSTGRESQL(PQbackendPID)
  if test "x$enable_postgresql" = "xyes"; then
    A68G_AC_POSTGRESQL(PQclear)
    A68G_AC_POSTGRESQL(PQcmdStatus)
    A68G_AC_POSTGRESQL(PQcmdTuples)
    A68G_AC_POSTGRESQL(PQconnectdb)
    A68G_AC_POSTGRESQL(PQdb)
    A68G_AC_POSTGRESQL(PQerrorMessage)
    A68G_AC_POSTGRESQL(PQexec)
    A68G_AC_POSTGRESQL(PQfformat)
    A68G_AC_POSTGRESQL(PQfinish)
    A68G_AC_POSTGRESQL(PQfname)
    A68G_AC_POSTGRESQL(PQfnumber)
    A68G_AC_POSTGRESQL(PQgetisnull)
    A68G_AC_POSTGRESQL(PQgetvalue)
    A68G_AC_POSTGRESQL(PQhost)
    A68G_AC_POSTGRESQL(PQnfields)
    A68G_AC_POSTGRESQL(PQntuples)
    A68G_AC_POSTGRESQL(PQoptions)
    A68G_AC_POSTGRESQL(PQparameterStatus)
    A68G_AC_POSTGRESQL(PQpass)
    A68G_AC_POSTGRESQL(PQport)
    A68G_AC_POSTGRESQL(PQprotocolVersion)
    A68G_AC_POSTGRESQL(PQreset)
    A68G_AC_POSTGRESQL(PQresultErrorMessage)
    A68G_AC_POSTGRESQL(PQresultStatus)
    A68G_AC_POSTGRESQL(PQserverVersion)
    A68G_AC_POSTGRESQL(PQsocket)
    A68G_AC_POSTGRESQL(PQstatus)
    A68G_AC_POSTGRESQL(PQtty)
    A68G_AC_POSTGRESQL(PQuser)
  fi
  if test "x$enable_postgresql" = "xyes"; then
    AC_CHECK_LIB([pq], [PQbackendPID])
    AC_DEFINE(HAVE_POSTGRESQL, 1, [Define this if a good PostgreSQL installation was detected]) 
  fi
fi

if test "x$enable_compiler" = "xyes"; then
  AC_MSG_NOTICE([Dynamic loader...])
  AC_CHECK_HEADERS([dlfcn.h])
  A68G_AC_DL(dlopen)
  if test "x$enable_compiler" = "xyes"; then
    A68G_AC_DL(dlsym)
    A68G_AC_DL(dlerror)
    A68G_AC_DL(dlclose)
  fi
  if test "x$enable_compiler" = "xyes"; then
    AC_CHECK_LIB([dl], [dlopen])
    AC_DEFINE(HAVE_DL, 1, [Define this if a good DL installation was detected]) 
  fi
fi

#
# Generate files.
#

AC_CONFIG_FILES([Makefile])
AC_OUTPUT
AC_MSG_NOTICE([])
AC_MSG_NOTICE([AC_PACKAGE_NAME-AC_PACKAGE_VERSION by Marcel van der Veer <algol68g@xs4all.nl>])
if test "x$a68g_exists" = "xyes"; then
  AC_MSG_NOTICE([AC_PACKAGE_NAME already exists on this system])
fi
if test "x$enable_parallel" = "xyes"; then
  AC_MSG_NOTICE([building with parallel clause])
fi
if test "x$enable_plotutils" = "xyes"; then
  AC_MSG_NOTICE([building with GNU plotutils])
fi
if test "x$enable_gsl" = "xyes"; then
  AC_MSG_NOTICE([building with GNU scientific library])
fi
if test "x$enable_postgresql" = "xyes"; then
  AC_MSG_NOTICE([building with PostgreSQL])
fi
if test "x$enable_compiler" = "xyes"; then
  AC_MSG_NOTICE([building compiler-interpreter])
else
  AC_MSG_NOTICE([building interpreter-only])
fi
AC_MSG_NOTICE([now type 'make' optionally followed by 'make check' or 'make install'])
