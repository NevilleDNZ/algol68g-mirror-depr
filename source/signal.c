/*!
\file signal.c
\brief signal handlers
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2010 J. Marcel van der Veer <algol68g@xs4all.nl>.

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

#include "config.h"
#include "diagnostics.h"
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
\brief raise SYSREQUEST so you get to a monitor
\param i dummy
**/

static void sigint_handler (int i)
{
  (void) i;
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  if (!(STATUS_TEST (program.top_node, BREAKPOINT_INTERRUPT_MASK) || in_monitor)) {
    STATUS_SET (program.top_node, BREAKPOINT_INTERRUPT_MASK);
    genie_break (program.top_node);
  }
}

#if ! defined ENABLE_WIN32

/*!
\brief signal reading from disconnected terminal
\param i dummy
**/

static void sigttin_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "background process attempts reading from disconnected terminal", NULL);
}

/*!
\brief signal broken pipe
\param i dummy
**/

static void sigpipe_handler (int i)
{
  (void) i;
  ABEND (A68_TRUE, "forked process has broken the pipe", NULL);
}

/*!
\brief signal alarm - time limit check
\param i dummy
**/

static void sigalrm_handler (int i)
{
  (void) i;
  if (in_execution && !in_monitor) {
    double _m_t = (double) program.options.time_limit;
    if (_m_t > 0 && (seconds () - cputime_0) > _m_t) {
      diagnostic_node (A68_RUNTIME_ERROR, (NODE_T *) last_unit, ERROR_TIME_LIMIT_EXCEEDED);
      exit_genie ((NODE_T *) last_unit, A68_RUNTIME_ERROR);
    }
  }
  (void) alarm (1);
}

#endif

/*!
\brief install_signal_handlers
**/

void install_signal_handlers (void)
{
  ABEND (signal (SIGINT, sigint_handler) == SIG_ERR, "cannot install SIGINT handler", NULL);
  ABEND (signal (SIGSEGV, sigsegv_handler) == SIG_ERR, "cannot install SIGSEGV handler", NULL);
#if ! defined ENABLE_WIN32
  ABEND (signal (SIGALRM, sigalrm_handler) == SIG_ERR, "cannot install SIGALRM handler", NULL);
  ABEND (signal (SIGPIPE, sigpipe_handler) == SIG_ERR, "cannot install SIGPIPE handler", NULL);
  ABEND (signal (SIGTTIN, sigttin_handler) == SIG_ERR, "cannot install SIGTTIN handler", NULL);
#endif
}
