/*!
\file signal.c
\brief signal handlers
*/

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


#include "algol68g.h"
#include "genie.h"

/*!
\brief signal reading for segment violation
\param i dummy
**/

static void sigsegv_handler (int i)
{
  (void) i;
  exit (EXIT_FAILURE);
  return;
}

/*!
\brief signal reading from disconnected terminal
\param i dummy
**/

static void sigttin_handler (int i)
{
  (void) i;
  ABNORMAL_END (A68_TRUE, "background process attempts reading from disconnected terminal", NULL);
}

/*!
\brief signal broken pipe
\param i dummy
**/

static void sigpipe_handler (int i)
{
  (void) i;
  ABNORMAL_END (A68_TRUE, "forked process has broken the pipe", NULL);
}

/*!
\brief raise SYSREQUEST so you get to a monitor
\param i dummy
**/

static void sigint_handler (int i)
{
  (void) i;
  ABNORMAL_END (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  if (!((MASK (a68_prog.top_node) & BREAKPOINT_INTERRUPT_MASK) || in_monitor)) {
    MASK (a68_prog.top_node) |= BREAKPOINT_INTERRUPT_MASK;
    genie_break (a68_prog.top_node);
  }
}

/*!
\brief install_signal_handlers
**/

void install_signal_handlers (void)
{
  ABNORMAL_END (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  ABNORMAL_END (signal (SIGSEGV, sigsegv_handler) == SIG_ERR, "cannot install SIGSEGV handler", NULL);
#if ! defined ENABLE_WIN32
  ABNORMAL_END (signal (SIGPIPE, sigpipe_handler) == SIG_ERR, "cannot install SIGPIPE handler", NULL);
  ABNORMAL_END (signal (SIGTTIN, sigttin_handler) == SIG_ERR, "cannot install SIGTTIN handler", NULL);
#endif
}
