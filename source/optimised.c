/*!
\file optimised.c
\brief routines executing primitive A68 actions
*/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2008 J. Marcel van der Veer <algol68g@xs4all.nl>.

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
#include "inline.h"
#include "mp.h"

/*!
\brief INT addition
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_plus_int (NODE_T * p)
{
  A68_INT *x, *y;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_INT, y);
  TEST_INT_ADDITION (op, VALUE (x), VALUE (y));
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (x) + VALUE (y), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief INT addition
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_plus_int_constant (NODE_T * p)
{
  A68_INT *i, *j;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, i);
  j = (A68_INT *) (((((v->genie).propagator).source)->genie).constant);
  TEST_INT_ADDITION (op, VALUE (i), VALUE (j));
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (i) + VALUE (j), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief INT subtraction
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_minus_int (NODE_T * p)
{
  A68_INT *x, *y;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_INT, y);
  TEST_INT_ADDITION (op, VALUE (x), -VALUE (y));
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (x) - VALUE (y), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief INT subtraction
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_minus_int_constant (NODE_T * p)
{
  A68_INT *i, *j;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, i);
  j = (A68_INT *) (((((v->genie).propagator).source)->genie).constant);
  TEST_INT_ADDITION (op, VALUE (i), -VALUE (j));
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (i) - VALUE (j), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief INT multiplication
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_times_int (NODE_T * p)
{
  A68_INT *x, *y;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_INT, y);
  TEST_INT_MULTIPLICATION (op, VALUE (x), VALUE (y));
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (x) * VALUE (y), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief INT division
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_over_int (NODE_T * p)
{
  A68_INT *x, *y;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_INT, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_INT, y);
  if (VALUE (y) == 0) {
    diagnostic_node (A68_RUNTIME_ERROR, op, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, VALUE (x) / VALUE (y), A68_INT);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief REAL addition
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_plus_real (NODE_T * p)
{
  A68_REAL *x, *y;
  double z;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_REAL, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_REAL, y);
  z = VALUE (x) + VALUE (y);
  TEST_REAL_REPRESENTATION (op, z);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, z, A68_REAL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief REAL subtraction
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_minus_real (NODE_T * p)
{
  A68_REAL *x, *y;
  double z;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_REAL, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_REAL, y);
  z = VALUE (x) - VALUE (y);
  TEST_REAL_REPRESENTATION (op, z);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, z, A68_REAL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief REAL multiplication
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_times_real (NODE_T * p)
{
  A68_REAL *x, *y;
  double z;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_REAL, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_REAL, y);
  z = VALUE (x) * VALUE (y);
  TEST_REAL_REPRESENTATION (op, z);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, z, A68_REAL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief REAL division
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_formula_div_real (NODE_T * p)
{
  A68_REAL *x, *y;
  double z;
  NODE_T *u = SUB (p), *op = NEXT (u), *v = NEXT (op);
  ADDR_T pop_sp = stack_pointer;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (u, A68_REAL, x);
  GENIE_GET_UNIT_ADDRESS (v, A68_REAL, y);
  if (VALUE (y) == 0) {
    diagnostic_node (A68_RUNTIME_ERROR, op, ERROR_DIVISION_BY_ZERO, MODE (REAL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  z = VALUE (x) / VALUE (y);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, z, A68_REAL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation_is_nil (NODE_T * p)
{
  NODE_T *lhs = SUB (p);
  ADDR_T pop_sp = stack_pointer;
  A68_REF *x;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (lhs, A68_REF, x);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, IS_NIL (*x), A68_BOOL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

/*!
\brief push equality of two REFs
\param p position in tree
\return a propagator for this action
**/

PROPAGATOR_T genie_identity_relation_isnt_nil (NODE_T * p)
{
  NODE_T *lhs = SUB (p);
  ADDR_T pop_sp = stack_pointer;
  A68_REF *x;
  UP_SWEEP_SEMA;
  GENIE_GET_UNIT_ADDRESS (lhs, A68_REF, x);
  stack_pointer = pop_sp;
  PUSH_PRIMITIVE (p, !IS_NIL (*x), A68_BOOL);
  DOWN_SWEEP_SEMA;
  return (PROPAGATOR (p));
}

#define COMPARE(NAME, TYPE, OP)\
PROPAGATOR_T NAME (NODE_T * p)\
{\
  TYPE *x, *y;\
  NODE_T *u = SUB (p), *v = NEXT_NEXT (u);\
  ADDR_T pop_sp = stack_pointer;\
  UP_SWEEP_SEMA;\
  GENIE_GET_UNIT_ADDRESS (u, TYPE, x);\
  GENIE_GET_UNIT_ADDRESS (v, TYPE, y);\
  stack_pointer = pop_sp;\
  PUSH_PRIMITIVE (p, VALUE (x) OP VALUE (y), A68_BOOL);\
  DOWN_SWEEP_SEMA;\
  return (PROPAGATOR (p));\
}

COMPARE (genie_formula_eq_int, A68_INT, ==);
COMPARE (genie_formula_ne_int, A68_INT, !=);
COMPARE (genie_formula_lt_int, A68_INT, <);
COMPARE (genie_formula_le_int, A68_INT, <=);
COMPARE (genie_formula_gt_int, A68_INT, >);
COMPARE (genie_formula_ge_int, A68_INT, >=);
COMPARE (genie_formula_eq_real, A68_REAL, ==);
COMPARE (genie_formula_ne_real, A68_REAL, !=);
COMPARE (genie_formula_lt_real, A68_REAL, <);
COMPARE (genie_formula_le_real, A68_REAL, <=);
COMPARE (genie_formula_gt_real, A68_REAL, >);
COMPARE (genie_formula_ge_real, A68_REAL, >=)
