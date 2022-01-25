#
# Algol 68 Genie 3.0.3, configure.ac
#
# To configure a68g for the widest range of platforms, issue: 
#   ./configure --enable-generic
#
# Following optional libraries are suggested for a full build on debian or derived linux distributions (Ubuntu, Raspberry Pi):
#   sudo apt install libncurses-dev libreadline-dev libmpfr libgmp libgsl-dev r-mathlib libplot-dev libghc-postgresql-libpq-dev
#

AC_INIT([algol68g], [3.0.3], [Marcel van der Veer <algol68g@xs4all.nl>])
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

# Configure path for the GNU Scientific Library
# Christopher R. Gabriel <cgabriel@linux.it>, April 2000

AC_DEFUN([AX_PATH_GSL],
[
AC_ARG_WITH(gsl-prefix,[  --with-gsl-prefix=PFX   Prefix where GSL is installed (optional)],
            gsl_prefix="$withval", gsl_prefix="")
AC_ARG_WITH(gsl-exec-prefix,[  --with-gsl-exec-prefix=PFX Exec prefix where GSL is installed (optional)],
            gsl_exec_prefix="$withval", gsl_exec_prefix="")
AC_ARG_ENABLE(gsltest, [  --disable-gsltest       Do not try to compile and run a test GSL program],
		    , enable_gsltest=yes)

  if test "x${GSL_CONFIG+set}" != xset ; then
     if test "x$gsl_prefix" != x ; then
         GSL_CONFIG="$gsl_prefix/bin/gsl-config"
     fi
     if test "x$gsl_exec_prefix" != x ; then
        GSL_CONFIG="$gsl_exec_prefix/bin/gsl-config"
     fi
  fi

  AC_PATH_PROG(GSL_CONFIG, gsl-config, no)
  min_gsl_version=ifelse([$1], ,0.2.5,$1)
  AC_MSG_CHECKING(for GSL version >= $min_gsl_version)
  no_gsl=""
  if test "$GSL_CONFIG" = "no" ; then
    no_gsl=yes
  else
    GSL_CFLAGS=`$GSL_CONFIG --cflags`
    GSL_LIBS=`$GSL_CONFIG --libs`

    gsl_major_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\).*/\1/'`
    if test "x${gsl_major_version}" = "x" ; then
       gsl_major_version=0
    fi

    gsl_minor_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\).*/\2/'`
    if test "x${gsl_minor_version}" = "x" ; then
       gsl_minor_version=0
    fi

    gsl_micro_version=`$GSL_CONFIG --version | \
           sed 's/^\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\)\.\{0,1\}\([[0-9]]*\).*/\3/'`
    if test "x${gsl_micro_version}" = "x" ; then
       gsl_micro_version=0
    fi

    if test "x$enable_gsltest" = "xyes" ; then
      ac_save_CFLAGS="$CFLAGS"
      ac_save_LIBS="$LIBS"
      CFLAGS="$CFLAGS $GSL_CFLAGS"
      LIBS="$LIBS $GSL_LIBS"

      rm -f conf.gsltest
      AC_TRY_RUN([
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

char* my_strdup (const char *str);

char*
my_strdup (const char *str)
{
  char *new_str;
  
  if (str)
    {
      new_str = (char *)malloc ((strlen (str) + 1) * sizeof(char));
      strcpy (new_str, str);
    }
  else
    new_str = NULL;
  
  return new_str;
}

int main (void)
{
  int major = 0, minor = 0, micro = 0;
  int n;
  char *tmp_version;

  system ("touch conf.gsltest");

  /* HP/UX 9 (%@#!) writes to sscanf strings */
  tmp_version = my_strdup("$min_gsl_version");

  n = sscanf(tmp_version, "%d.%d.%d", &major, &minor, &micro) ;

  if (n != 2 && n != 3) {
     printf("%s, bad version string\n", "$min_gsl_version");
     exit(1);
   }

   if (($gsl_major_version > major) ||
      (($gsl_major_version == major) && ($gsl_minor_version > minor)) ||
      (($gsl_major_version == major) && ($gsl_minor_version == minor) && ($gsl_micro_version >= micro)))
     { 
       exit(0);
     }   
   else
     {
       exit(1);
     }
}

],, no_gsl=yes,[echo $ac_n "cross compiling; assumed OK... $ac_c"])
       CFLAGS="$ac_save_CFLAGS"
       LIBS="$ac_save_LIBS"
     fi
  fi
  if test "x$no_gsl" = x ; then
     AC_MSG_RESULT(yes)
     ifelse([$2], , :, [$2])     
  else
     AC_MSG_RESULT(no)
     if test "$GSL_CONFIG" = "no" ; then
       echo "*** The gsl-config script installed by GSL could not be found"
       echo "*** If GSL was installed in PREFIX, make sure PREFIX/bin is in"
       echo "*** your path, or set the GSL_CONFIG environment variable to the"
       echo "*** full path to gsl-config."
     else
       if test -f conf.gsltest ; then
        :
       else
          echo "*** Could not run GSL test program, checking why..."
          CFLAGS="$CFLAGS $GSL_CFLAGS"
          LIBS="$LIBS $GSL_LIBS"
          AC_TRY_LINK([
#include <stdio.h>
],      [ return 0; ],
        [ echo "*** The test program compiled, but did not run. This usually means"
          echo "*** that the run-time linker is not finding GSL or finding the wrong"
          echo "*** version of GSL. If it is not finding GSL, you'll need to set your"
          echo "*** LD_LIBRARY_PATH environment variable, or edit /etc/ld.so.conf to point"
          echo "*** to the installed location  Also, make sure you have run ldconfig if that"
          echo "*** is required on your system"
	  echo "***"
          echo "*** If you have an old version installed, it is best to remove it, although"
          echo "*** you may also be able to get things to work by modifying LD_LIBRARY_PATH"],
        [ echo "*** The test program failed to compile or link. See the file config.log for the"
          echo "*** exact error that occured. This usually means GSL was incorrectly installed"
          echo "*** or that you have moved GSL since it was installed. In the latter case, you"
          echo "*** may want to edit the gsl-config script: $GSL_CONFIG" ])
          CFLAGS="$ac_save_CFLAGS"
          LIBS="$ac_save_LIBS"
       fi
     fi
#     GSL_CFLAGS=""
#     GSL_LIBS=""
     ifelse([$3], , :, [$3])
  fi
  AC_SUBST(GSL_CFLAGS)
  AC_SUBST(GSL_LIBS)
  rm -f conf.gsltest
])

AU_ALIAS([AM_PATH_GSL], [AX_PATH_GSL])
#
# Platform ids.
#

AC_MSG_NOTICE([host system...])
AC_CANONICAL_BUILD
AC_CANONICAL_HOST
AC_CANONICAL_TARGET

enable_haiku=no
AC_MSG_CHECKING([platform])
case "$host" in
#
# Linux.
#
# aarch64*-*-linux* is for RaspberryPi-4 on ARM-64 - otherwise http/tcp isnt found
*86-*-gnu | *86_64-*-gnu | *86-*-linux* | *86_64-*-linux* | arm*-*-linux* | aarch*-*-linux*) 
  AC_DEFINE(BUILD_LINUX, 1, [Define this if LINUX was detected]) 
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
  AC_DEFINE(BUILD_BSD, 1, [Define this if DARWIN was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_WARN([configuring interpreter-only])
  AC_MSG_RESULT([mac os x])
  ;;
#
# FreeBSD.
#
*86-*-freebsd* | *86_64-*-freebsd*) 
  AC_DEFINE(BUILD_BSD, 1, [Define this if FreeBSD was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_WARN([configuring interpreter-only])
  AC_MSG_RESULT([freebsd])
  ;;
#
# NetBSD.
#
*86-*-netbsd* | *86_64-*-netbsd*) 
  AC_DEFINE(BUILD_BSD, 1, [Define this if NetBSD was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_WARN([configuring interpreter-only])
  AC_MSG_RESULT([netbsd])
  ;;
#
# OpenBSD.
#
*86-*-openbsd* | *86_64-*-openbsd*) 
  AC_DEFINE(BUILD_BSD, 1, [Define this if OpenBSD was detected]) 
  AC_MSG_WARN([configuring interpreter-only])
  AC_MSG_RESULT([openbsd])
  enable_compiler=no
  ;;
#
# Haiku.
#
*86-*-haiku* | *86_64-*-haiku*) 
  AC_DEFINE(BUILD_HAIKU, 1, [Define this if HAIKU was detected]) 
  AC_DEFINE(HAVE_IEEE_754, 1, [Define this if IEEE_754 compliant]) 
  AC_MSG_RESULT([haiku])
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

AC_ARG_ENABLE(generic, [AS_HELP_STRING([--enable-generic], [build for legacy 32 bit systems (default is "no")])], , enable_generic=no) 
AC_ARG_ENABLE(compiler, [AS_HELP_STRING([--enable-compiler], [enable unit compiler (default is "yes")])], , enable_compiler=yes) 
AC_ARG_ENABLE(quadmath, [AS_HELP_STRING([--enable-quadmath], [if available, enable GCC quadmath library (default is "yes")])], , enable_quadmath=yes) 
AC_ARG_ENABLE(mathlib, [AS_HELP_STRING([--enable-mathlib], [if available, enable R mathlib library (default is "yes")])], , enable_mathlib=yes) 
AC_ARG_ENABLE(curses, [AS_HELP_STRING([--enable-curses], [if available, enable curses library (default is "yes")])], , enable_curses=yes) 
AC_ARG_ENABLE(gsl, [AS_HELP_STRING([--enable-gsl], [if available, enable GNU scientific library (default is "yes")])], , enable_gsl=yes) 
AC_ARG_ENABLE(mpfr, [AS_HELP_STRING([--enable-mpfr], [if available, enable GNU MPFR (default is "yes")])], , enable_mpfr=yes) 
AC_ARG_ENABLE(parallel, [AS_HELP_STRING([--enable-parallel], [enable Algol 68 parallel-clause (default is "yes")])], , enable_parallel=yes) 
AC_ARG_ENABLE(plotutils, [AS_HELP_STRING([--enable-plotutils], [if available, enable GNU plotting utilities (default is "yes")])], , enable_plotutils=yes) 
AC_ARG_ENABLE(postgresql, [AS_HELP_STRING([--enable-postgresql], [if available, enable PostgreSQL (default is "yes")])], , enable_postgresql=yes) 
AC_ARG_ENABLE(readline, [AS_HELP_STRING([--enable-readline], [if available, enable readline library (default is "yes")])], , enable_readline=yes) 
AC_ARG_ENABLE(standard-types, [AS_HELP_STRING([--enable-standard-types], [enable int*4 and real*8 modes (default is "yes")])], , enable_standard_types=yes) 
AC_ARG_ENABLE(long-types, [AS_HELP_STRING([--enable-long-types], [if available, enable int*8 and real*10, real*16 modes (default is "yes")])], , enable_long_types=yes) 
AC_ARG_ENABLE(pic, [AS_HELP_STRING([--enable-pic=option], [if using gcc, enable option to generate PIC (default is "-fPIC")])], , enable_pic="-fPIC") 
AC_ARG_ENABLE(arch, [AS_HELP_STRING([--enable-arch=cpu], [if using gcc, enable emitting architecture-tuned assembly code (default is "no")])], , enable_arch=no) 

if test "x$enable_generic" = "xyes"; then
  enable_standard_types=yes
  enable_long_types=no
fi

#
# Initialisation.
#

AC_MSG_NOTICE([initialising...])
AM_INIT_AUTOMAKE([subdir-objects serial-tests])
AC_PREFIX_DEFAULT(/usr/local)
AC_CONFIG_SRCDIR([src/include/a68g.h])
AC_CONFIG_HEADERS([a68g-config.h])
AC_PROG_INSTALL
AC_CHECK_PROG(a68g_exists, a68g, "yes")

#
# C compiler.
#

AC_MSG_NOTICE([C compiler...])
AC_LANG(C)
AC_PROG_CC([gcc clang])
# AC_PROG_CC([clang])
if test "x$GCC" != "xyes"; then
  a68g_ac_compiler=no
  AC_MSG_WARN([gcc is the preferred C compiler; configuring interpreter-only])
else
  AC_DEFINE(HAVE_GCC, 1, [Define this if GCC was detected]) 
  CFLAGS="-g -O2 -Wall"
  A68G_AC_PROG_CC_CFLAGS([-Wshadow])
  A68G_AC_PROG_CC_CFLAGS([-Wunused-variable])
  A68G_AC_PROG_CC_CFLAGS([-Wunused-parameter])
  A68G_AC_PROG_CC_CFLAGS([-Wno-long-long])
  A68G_AC_PROG_CC_CFLAGS([-fno-diagnostics-color])
#
# Test on gcc capabilities.
#
  AC_MSG_CHECKING([__attribute__((aligned())) supported])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [typedef int aint __attribute__((aligned(8)));])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   AC_MSG_FAILURE([stop -- C compiler does not support __attribute__aligned directive])],
  [])
  AC_MSG_CHECKING([attribute inline supported])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [inline void skip (void) {;}])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   AC_MSG_FAILURE([stop -- C compiler does not support __attribute__aligned directive])],
  [])
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
[AC_LANG_SOURCE([
#include <unistd.h>
#include <sys/types.h>
__off_t dummy;
])],
  [AC_MSG_RESULT([__off_t])],
  [AC_MSG_RESULT([off_t])
   AC_DEFINE(__off_t, off_t, [Define this if we have no __off_t])]
)

AC_MSG_CHECKING([__pid_t or pid_t])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE[
#include <unistd.h>
#include <sys/types.h>
__pid_t dummy;
])],
  [AC_MSG_RESULT([__pid_t])
   AC_DEFINE(a68_pid_t, __pid_t, [Define this if we have __pid_t])],
  [AC_MSG_RESULT([pid_t])
   AC_DEFINE(a68_pid_t, pid_t, [Define this if we have no __pid_t])]
)

AC_MSG_CHECKING([__mode_t or mode_t])
AC_COMPILE_IFELSE(
[AC_LANG_SOURCE([
#include <unistd.h>
#include <sys/types.h>
__mode_t dummy;
])],
  [AC_MSG_RESULT([__mode_t])],
  [AC_MSG_RESULT([mode_t])
   AC_DEFINE(__mode_t, mode_t, [Define this if we have no __mode_t])]
)

#
# Extra include directories.
#

AC_MSG_NOTICE([extra include directories...])

# R on Debian
# 
# if test -d /usr/share/R/include; then
#   AC_DEFINE(HAVE_USR_SHARE_R_INCLUDE, 1, [Define this if /usr/share/R/include was detected]) 
#   CFLAGS="$CFLAGS -I/usr/share/R/include"
#   CPPFLAGS="$CPPFLAGS -I/usr/share/R/include"
#   CXXFLAGS="$CXXFLAGS -I/usr/share/R/include"
#   LDFLAGS="$LDFLAGS -L/usr/share/R/lib"
# fi

# Postgresql

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

if test -d /usr/include/postgresql; then
  AC_DEFINE(HAVE_USR_INCLUDE_POSTGRESQL, 1, [Define this if /usr/include/postgresql was detected]) 
  CFLAGS="$CFLAGS -I/usr/include/postgresql"
  CPPFLAGS="$CPPFLAGS -I/usr/include/postgresql"
  CXXFLAGS="$CXXFLAGS -I/usr/include/postgresql"
  LDFLAGS="$LDFLAGS -L/usr/lib"
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

AC_CHECK_HEADERS([assert.h complex.h ctype.h errno.h fcntl.h fenv.h float.h limits.h netdb.h netinet/in.h regex.h setjmp.h signal.h stdarg.h stddef.h stdio.h stdlib.h sys/ioctl.h sys/resource.h sys/socket.h sys/time.h termios.h time.h])

#
# Functions we expect
#

AC_MSG_NOTICE([standard functions...])
AC_CHECK_FUNCS(malloc)
AC_CHECK_FUNCS(aligned_alloc)
AC_CHECK_FUNCS(free)
AC_CHECK_FUNCS(setjmp)
AC_CHECK_FUNCS(longjmp)
AC_CHECK_FUNCS(memcpy)
AC_CHECK_FUNCS(memmove)
AC_CHECK_FUNCS(memset)
AC_CHECK_FUNCS(strcmp)
AC_CHECK_FUNCS(strncmp)
AC_CHECK_FUNCS(strncpy)
AC_CHECK_FUNCS(printf)
AC_CHECK_FUNCS(fprintf)
AC_CHECK_FUNCS(snprintf)
AC_CHECK_FUNCS(signal)
AC_CHECK_FUNCS(exit)
AC_CHECK_FUNCS(sqrt)
AC_CHECK_FUNCS(csqrt)
AC_CHECK_FUNCS(isfinite)
AC_CHECK_FUNCS(finite)
AC_CHECK_FUNCS(isnan)
AC_CHECK_FUNCS(isinf)

AC_MSG_NOTICE([optional headers and libraries...])

if test "x$enable_standard_types" = "xyes"; then
  AC_MSG_CHECKING([int is 32 bit])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (int) != 4;])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   enable_long_types=no],
  [])
  AC_MSG_CHECKING([unsigned is 32 bit])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (unsigned) != 4;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  AC_MSG_CHECKING([double is 64 bit])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (double) != 8;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  AC_MSG_CHECKING([uint64_t is 64 bit])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <stdint.h>], [return sizeof (uint64_t) != 8;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_standard_types=no
     enable_long_types=no],
  [])
fi

if test "x$enable_standard_types" = "xno"; then
  AC_MSG_FAILURE([stop -- unexpected lengths for int and/or double and/or uint64_t])
fi

if test "x$enable_long_types" = "xyes"; then
  AC_MSG_CHECKING([64-bit long long int is available])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (long long) != 8;])],
  [AC_MSG_RESULT(yes)],
  [AC_MSG_RESULT(no)
   enable_long_types=no],
  [])
  AC_MSG_CHECKING([64-bit long long unsigned is available])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (long long unsigned) != 8;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  AC_MSG_CHECKING([80-bit __float80 is available])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (__float80) != 16;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  AC_MSG_CHECKING([80-bit __float80 has 64-bit mantissa])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([#include <float.h>], [return LDBL_MANT_DIG != 64;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  AC_MSG_CHECKING([128-bit __float128 is available])
  AC_RUN_IFELSE([AC_LANG_PROGRAM([], [return sizeof (__float128) != 16;])],
    [AC_MSG_RESULT(yes)],
    [AC_MSG_RESULT(no)
     enable_long_types=no],
  [])
  if test "x$enable_long_types" = "xyes"; then
    AC_DEFINE(HAVE_LONG_TYPES, 1, [Define this if a good INT*8/REAL*10/REAL*16 installation was detected]) 
  fi
fi

if test "x$enable_long_types" = "xyes"; then
  if test "x$enable_quadmath" = "xyes"; then
    AC_MSG_NOTICE([quadmath...])
    AC_CHECK_HEADERS([quadmath.h], [], [enable_quadmath=no], [])
    AC_CHECK_LIB([quadmath], [lgammaq], [], [enable_quadmath=no])
    if test "x$enable_quadmath" = "xyes"; then
      AC_DEFINE(HAVE_QUADMATH, 1, [Define this if a good quadmath installation was detected]) 
    fi
  fi
else
  AC_MSG_NOTICE([quadmath ignored for this system])
  enable_quadmath=no
fi

if test "x$enable_long_types" = "xyes"; then
  if test "x$enable_mpfr" = "xyes"; then
    AC_MSG_NOTICE([GNU MPFR...])
    AC_CHECK_LIB([gmp], [__gmpz_init], [], [enable_mpfr=no])
    if test "x$enable_mpfr" = "xyes"; then
      AC_CHECK_HEADERS([mpfr.h])
      if test "x$enable_mpfr" = "xyes"; then
        AC_CHECK_LIB([mpfr], [mpfr_gamma], [], [enable_mpfr=no])
        if test "x$enable_mpfr" = "xyes"; then
          AC_CHECK_LIB([mpfr], [mpfr_gamma_inc], [
            AC_DEFINE(HAVE_GNU_MPFR, 1, [Define this if a recent GNU MPFR installation was detected]) 
          ], [])
        fi
      fi
    fi
  fi
else
  AC_MSG_NOTICE([GNU MPFR ignored for this system])
  enable_mpfr=no
fi

if test "x$enable_parallel" = "xyes"; then
  AC_MSG_NOTICE([POSIX pthreads...])
  AC_CHECK_HEADERS([pthread.h], [], [enable_parallel=no], [])
  AC_CHECK_LIB([pthread], [pthread_attr_getstacksize], [], [enable_parallel=no])
  if test "x$enable_parallel" = "xyes"; then
    AC_DEFINE(BUILD_PARALLEL_CLAUSE, 1, [Define this if a good pthread installation was detected]) 
  fi
fi

if test "x$enable_mathlib" = "xyes"; then
  AC_MSG_NOTICE([mathlib...])
  AC_CHECK_HEADERS([Rmath.h], [], [enable_mathlib=no], 
    [#define MATHLIB_STANDALONE
     #include <Rmath.h>
    ])
  AC_CHECK_LIB([Rmath], [ptukey], [], [enable_mathlib=no])
  if test "x$enable_mathlib" = "xyes"; then
    AC_DEFINE(HAVE_MATHLIB, 1, [Define this if a good mathlib installation was detected]) 
  fi
fi

if test "x$enable_gsl" = "xyes"; then
  AC_MSG_NOTICE([GNU Scientific Library...])
  AX_PATH_GSL(2.5, [enable_gsl=yes], [enable_gsl=no])
  if test "x$enable_gsl" = "xyes"; then
    AC_CHECK_LIB([gslcblas], [cblas_dgemm], [], [enable_gsl=no])
    AC_CHECK_LIB([gsl], [gsl_blas_ddot], [], [enable_gsl=no])
    AC_CHECK_HEADERS([gsl/gsl_blas.h gsl/gsl_complex.h gsl/gsl_complex_math.h gsl/gsl_errno.h gsl/gsl_fft_complex.h gsl/gsl_integration.h gsl/gsl_linalg.h gsl/gsl_math.h gsl/gsl_matrix.h gsl/gsl_permutation.h gsl/gsl_sf.h gsl/gsl_vector.h gsl/gsl_version.h], [], [enable_gsl=no], [])
  fi
  if test "x$enable_gsl" = "xyes"; then
    AC_DEFINE(HAVE_GSL, 1, [Define this if a good GNU GSL installation was detected])
  fi
fi

if test "x$enable_plotutils" = "xyes"; then
  AC_MSG_NOTICE([GNU plotutils...])
  AC_CHECK_HEADERS([plot.h], [], [enable_plotutils=no], [])
  AC_CHECK_LIB([plot], [pl_alabel_r], [], [enable_plotutils=no])
  if test "x$enable_plotutils" = "xyes"; then
    AC_DEFINE(HAVE_GNU_PLOTUTILS, 1, [Define this if a good GNU plotutils installation was detected]) 
  fi
fi

if test "x$enable_curses" = "xyes"; then
  AC_MSG_NOTICE([curses...])
  AC_CHECK_HEADERS([curses.h], [], [enable_curses=no], [])
  if test "x$enable_curses" = "xno"; then
    AC_CHECK_HEADERS([ncurses/curses.h], [enable_curses=yes], [], [])
  fi
  if test "x$enable_curses" = "xyes"; then
    AC_CHECK_LIB([tinfo], [stdscr], [], [enable_curses=no])
    AC_CHECK_LIB([tic], [stdscr], [], [enable_curses=no])
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

if test "x$enable_postgresql" = "xyes"; then
  AC_MSG_NOTICE([PostgreSQL...])
  AC_CHECK_HEADERS([libpq-fe.h], [], [enable_postgresql=no], [])
  AC_CHECK_LIB([pq], [PQbackendPID], [], [enable_postgresql=no])
  if test "x$enable_postgresql" = "xyes"; then
    AC_DEFINE(HAVE_POSTGRESQL, 1, [Define this if a good PostgreSQL installation was detected]) 
  fi
fi

if test "x$enable_compiler" = "xyes"; then
  libdl_found=no
  AC_MSG_NOTICE([Dynamic loader via libdl...])
  AC_CHECK_HEADERS([dlfcn.h])
  AC_CHECK_LIB([dl], [dlopen], [], enable_compiler=no)
  if test "x$enable_compiler" = "xyes"; then
    AC_DEFINE(HAVE_DL, 1, [Define this if a good DL installation was detected]) 
    libdl_found=yes
  fi
fi

# Are we on Haiku?
if test "x$enable_compiler" = "xyes"; then
  if test "x$libdl_found" = "xno"; then
    AC_MSG_NOTICE([Dynamic loader via libroot...])
    AC_CHECK_HEADERS([dlfcn.h])
    AC_CHECK_LIB([root], [dlopen], [], enable_compiler=no)
    if test "x$enable_compiler" = "xyes"; then
      AC_DEFINE(HAVE_DL, 1, [Define this if a good DL installation was detected]) 
    fi
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
if test "x$enable_quadmath" = "xyes"; then
  if test "x$enable_long_types" = "xyes"; then
    AC_MSG_NOTICE([building with hardware support for long modes])
    AC_MSG_NOTICE([building with quadmath])
  fi
fi
if test "x$enable_parallel" = "xyes"; then
  AC_MSG_NOTICE([building with parallel clause])
fi
if test "x$enable_mpfr" = "xyes"; then
  AC_MSG_NOTICE([building with GNU MPFR])
fi
if test "x$enable_mpfr" = "xno"; then
  if test "x$GCC" = "xyes"; then
    AC_MSG_NOTICE([GNU MPFR found but not used])
  fi
fi
if test "x$enable_mathlib" = "xyes"; then
  AC_MSG_NOTICE([building with R mathlib])
fi
if test "x$enable_gsl" = "xyes"; then
  AC_MSG_NOTICE([building with GNU Scientific Library])
fi
if test "x$enable_curses" = "xyes"; then
  AC_MSG_NOTICE([building with curses])
fi
if test "x$enable_plotutils" = "xyes"; then
  AC_MSG_NOTICE([building with GNU plotutils])
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
