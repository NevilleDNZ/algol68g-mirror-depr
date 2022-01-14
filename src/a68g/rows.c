//! @file rows.c
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

// Operators for ROWS.

//! @brief OP ELEMS = (ROWS) INT

void genie_monad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  GET_DESCRIPTOR (x, t, &z);
  PUSH_VALUE (p, get_row_size (t, DIM (x)), A68_INT);
}

//! @brief OP LWB = (ROWS) INT

void genie_monad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  GET_DESCRIPTOR (x, t, &z);
  PUSH_VALUE (p, LWB (t), A68_INT);
}

//! @brief OP UPB = (ROWS) INT

void genie_monad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  GET_DESCRIPTOR (x, t, &z);
  PUSH_VALUE (p, UPB (t), A68_INT);
}

//! @brief OP ELEMS = (INT, ROWS) INT

void genie_dyad_elems (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t, *u;
  A68_INT k;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  u = &(t[VALUE (&k) - 1]);
  PUSH_VALUE (p, ROW_SIZE (u), A68_INT);
}

//! @brief OP LWB = (INT, ROWS) INT

void genie_dyad_lwb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_VALUE (p, LWB (&(t[VALUE (&k) - 1])), A68_INT);
}

//! @brief OP UPB = (INT, ROWS) INT

void genie_dyad_upb (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *x;
  A68_TUPLE *t;
  A68_INT k;
  POP_REF (p, &z);
// Decrease pointer since a UNION is on the stack.
  DECREMENT_STACK_POINTER (p, A68_UNION_SIZE);
  CHECK_REF (p, z, M_ROWS);
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (x, t, &z);
  if (VALUE (&k) < 1 || VALUE (&k) > DIM (x)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_DIMENSION, (int) VALUE (&k));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_VALUE (p, UPB (&(t[VALUE (&k) - 1])), A68_INT);
}
