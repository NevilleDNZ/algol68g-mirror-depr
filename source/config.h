/*!
\file config.h
\brief contains manual configuration switches
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2006 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 2 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
*/

/*
This file defines system dependencies that cannot be detected (reliably) in an
automatic way. These dependencies are

   HAVE_SYSTEM_STACK_CHECK
   HAVE_MODIFIABLE_X_TITLE
   HAVE_UNIX_CLOCK
   WIN32_VERSION

Define or undefine the directives below depending on your system.
You can also set them when using `make' by using the CFLAGS parameter:

   make CFLAGS=-DHAVE_SYSTEM_STACK_CHECK

Refer to the file INSTALL.
*/

#ifndef A68G_CONFIG_H
#define A68G_CONFIG_H

/*
If you want the interpreter to check the system stack then define
HAVE_SYSTEM_STACK_CHECK. This check assumes that the stack grows linearly (either
upwards or downwards) and that the difference between (1) the address of a local
variable in the current stack frame and (2) the address of a local variable in
a stack frame at the start of execution of the Algol 68 program, is a measure
of system stack size.
*/

#ifndef HAVE_SYSTEM_STACK_CHECK
#define HAVE_SYSTEM_STACK_CHECK
#endif

/*
Did you edit GNU libplot so you can modify X Window titles? If yes then define
HAVE_MODIFIABLE_X_TITLE else undefine it.
*/

#ifndef HAVE_MODIFIABLE_X_TITLE
#undef HAVE_MODIFIABLE_X_TITLE
#endif

/*
HAVE_UNIX_CLOCK attempts getting usec resolution to the clock on UNIX systems.
This calls get_rusage, which does not work on some systems.
Undefining it means that clock () will be called, which is portable.
*/

#undef HAVE_UNIX_CLOCK

/*
When defining WIN32_VERSION you may want to change directives following the 
definition below to reflect your particular system.
HAVE_IEEE_754 is ok for Pentiums.
*/

#ifndef WIN32_VERSION
#undef WIN32_VERSION
#endif

#ifdef WIN32_VERSION
#undef HAVE_CURSES
#undef HAVE_PLOTUTILS
#undef HAVE_GSL
#undef HAVE_CURSES
#undef HAVE_POSTGRESQL
#undef HAVE_MODIFIABLE_X_TITLE
#undef HAVE_UNIX_CLOCK
#undef HAVE_POSIX_THREADS
#undef HAVE_HTTP
#undef HAVE_REGEX
#undef HAVE_POSTGRESQL
#define HAVE_IEEE_754 1
#ifdef HAVE_PLOTUTILS
#define X_DISPLAY_MISSING 1
#endif
#endif

/*
OS_2_VERSION and PRE_MACOS_X_VERSION have been decommitted since Mark 9.1.
*/

#endif
