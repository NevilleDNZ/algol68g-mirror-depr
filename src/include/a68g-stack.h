//! @file a68g-stack.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#if !defined (__A68G_STACK_H__)
#define __A68G_STACK_H__

// Macro's for stack checking. Since the stacks grow by small amounts at a time
// (A68 rows are in the heap), we check the stacks only at certain points: where
// A68 recursion may set in, or in the garbage collector. We check whether there
// still is sufficient overhead to make it to the next check.

#define TOO_COMPLEX "program too complex"

#define SYSTEM_STACK_USED (ABS ((int) (A68 (system_stack_offset) - &stack_offset)))
#define LOW_SYSTEM_STACK_ALERT(p) {\
  BYTE_T stack_offset;\
  if (A68 (stack_size) > 0 && SYSTEM_STACK_USED >= A68 (stack_limit)) {\
    if ((p) == NO_NODE) {\
      ABEND (A68_TRUE, TOO_COMPLEX, ERROR_STACK_OVERFLOW);\
    } else {\
      diagnostic (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
      exit_genie ((p), A68_RUNTIME_ERROR);\
  }}}

#define LOW_STACK_ALERT(p) {\
  LOW_SYSTEM_STACK_ALERT (p);\
  if ((p) != NO_NODE && (A68_FP >= A68 (frame_stack_limit) || A68_SP >= A68 (expr_stack_limit))) { \
    diagnostic (A68_RUNTIME_ERROR, (p), ERROR_STACK_OVERFLOW);\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }}

#endif
