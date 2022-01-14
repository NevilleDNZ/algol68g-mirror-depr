//! @file bool.c
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
#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"
#include "a68g-optimiser.h"
#include "a68g-double.h"

// BOOL operations.

// OP NOT = (BOOL) BOOL.

A68_MONAD (genie_not_bool, A68_BOOL, (BOOL_T) !);

//! @brief OP ABS = (BOOL) INT

void genie_abs_bool (NODE_T * p)
{
  A68_BOOL j;
  POP_OBJECT (p, &j, A68_BOOL);
  PUSH_VALUE (p, (VALUE (&j) ? 1 : 0), A68_INT);
}

#define A68_BOOL_DYAD(n, OP)\
void n (NODE_T * p) {\
  A68_BOOL *i, *j;\
  POP_OPERAND_ADDRESSES (p, i, j, A68_BOOL);\
  VALUE (i) = (BOOL_T) (VALUE (i) OP VALUE (j));\
}

A68_BOOL_DYAD (genie_and_bool, &);
A68_BOOL_DYAD (genie_or_bool, |);
A68_BOOL_DYAD (genie_xor_bool, ^);
A68_BOOL_DYAD (genie_eq_bool, ==);
A68_BOOL_DYAD (genie_ne_bool, !=);
