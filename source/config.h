/*!
\file config.h
\brief contains manual configuration switches
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
*/

/*
This file defines system dependencies that cannot be detected (reliably) in an
automatic way. These dependencies are

   ENABLE_SYS_STACK_CHECK
   ENABLE_X_TITLE
   ENABLE_UNIX_CLOCK
   ENABLE_WIN32

Define or undefine the directives below depending on your system.
You can also set them when using `make' by using the CFLAGS parameter:

   make CFLAGS=-DENABLE_SYS_STACK_CHECK

Refer to the file INSTALL.
*/

#if ! defined A68G_CONFIG_H
#define A68G_CONFIG_H

/*
If you want the interpreter to check the system stack then define
ENABLE_SYS_STACK_CHECK. This check assumes that the stack grows linearly (either
upwards or downwards) and that the difference between (1) the address of a local
variable in the current stack frame and (2) the address of a local variable in
a stack frame at the start of execution of the Algol 68 program, is a measure
of system stack size.
*/

#if ! defined ENABLE_SYS_STACK_CHECK
#define ENABLE_SYS_STACK_CHECK
#endif

/*
Did you edit GNU libplot so you can modify X Window titles? If yes then define
ENABLE_X_TITLE else undefine it.
*/

#if ! defined ENABLE_X_TITLE
#undef ENABLE_X_TITLE
#endif

/*
ENABLE_UNIX_CLOCK attempts getting usec resolution to the clock on UNIX systems.
This calls get_rusage, which does not work on some systems.
Undefining it means that clock () will be called, which is portable.
*/

#undef ENABLE_UNIX_CLOCK

/*
When defining ENABLE_WIN32 you may want to change directives following the 
definition below to reflect your particular system.
ENABLE_IEEE_754 is ok for Pentiums.
*/

#if ! defined ENABLE_WIN32
#undef ENABLE_WIN32
#endif

#if defined ENABLE_WIN32
#define ENABLE_GRAPHICS 1
#define ENABLE_IEEE_754 1
#define ENABLE_NUMERICAL 1
#define ENABLE_REGEX 1
#if defined ENABLE_GRAPHICS
#define X_DISPLAY_MISSING 1
#define POSTSCRIPT_MISSING 1
#endif
#undef ENABLE_CURSES
#undef ENABLE_HTTP
#undef ENABLE_PAR_CLAUSE
#undef ENABLE_POSTGRESQL
#undef ENABLE_POSTGRESQL
#undef ENABLE_UNIX_CLOCK
#undef ENABLE_X_TITLE
#endif

/*
OS_2_VERSION and PRE_MACOS_X_VERSION have been decommitted since version 1.9.1.
*/

#endif
