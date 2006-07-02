/*!
\file standard.c
\brief standard prelude implementation, except transput
*/

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
This file contains Algol68G's standard environ. Transput routines are not here.
Some of the LONG operations are generic for LONG and LONG LONG.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"

double cputime_0;

#define A68_MONAD(n, p, MODE, OP) void n (NODE_T * p) {\
  MODE *i;\
  POP_OPERAND_ADDRESS (p, i, MODE);\
  i->value = OP i->value;\
}

/*!
\brief math_rte math runtime error
\param p position in syntax tree, should not be NULL position in syntax tree, should not be NULL
\param z
\param m
\param t
**/

void math_rte (NODE_T * p, BOOL_T z, MOID_T * m, const char *t)
{
  if (z) {
    if (t == NULL) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH, m, NULL);
    } else {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_MATH_INFO, m, t);
    }
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief generic procedure for OP AND BECOMES (+:=, -:=, ...)
\param p position in syntax tree, should not be NULL
\param ref mode of destination
\param f pointer to function that performs operation
**/

void genie_f_and_becomes (NODE_T * p, MOID_T * ref, GENIE_PROCEDURE * f)
{
  MOID_T *mode = SUB (ref);
  int size = MOID_SIZE (mode);
  BYTE_T *src = STACK_OFFSET (-size), *addr;
  A68_REF *dst = (A68_REF *) STACK_OFFSET (-size - SIZE_OF (A68_REF));
  TEST_NIL (p, *dst, ref);
  addr = ADDRESS (dst);
  PUSH (p, addr, size);
  GENIE_CHECK_INITIALISATION (p, STACK_OFFSET (-size), mode);
  PUSH (p, src, size);
  f (p);
  POP (p, addr, size);
  DECREMENT_STACK_POINTER (p, size);
}

/* Environment enquiries. */

A68_ENV_INT (genie_int_lengths, 3);
A68_ENV_INT (genie_int_shorths, 1);
A68_ENV_INT (genie_real_lengths, 3);
A68_ENV_INT (genie_real_shorths, 1);
A68_ENV_INT (genie_complex_lengths, 3);
A68_ENV_INT (genie_complex_shorths, 1);
A68_ENV_INT (genie_bits_lengths, 3);
A68_ENV_INT (genie_bits_shorths, 1);
A68_ENV_INT (genie_bytes_lengths, 2);
A68_ENV_INT (genie_bytes_shorths, 1);
A68_ENV_INT (genie_int_width, INT_WIDTH);
A68_ENV_INT (genie_long_int_width, LONG_INT_WIDTH);
A68_ENV_INT (genie_longlong_int_width, LONGLONG_INT_WIDTH);
A68_ENV_INT (genie_real_width, REAL_WIDTH);
A68_ENV_INT (genie_long_real_width, LONG_REAL_WIDTH);
A68_ENV_INT (genie_longlong_real_width, LONGLONG_REAL_WIDTH);
A68_ENV_INT (genie_exp_width, EXP_WIDTH);
A68_ENV_INT (genie_long_exp_width, LONG_EXP_WIDTH);
A68_ENV_INT (genie_longlong_exp_width, LONGLONG_EXP_WIDTH);
A68_ENV_INT (genie_bits_width, BITS_WIDTH);
A68_ENV_INT (genie_long_bits_width, get_mp_bits_width (MODE (LONG_BITS)));
A68_ENV_INT (genie_longlong_bits_width, get_mp_bits_width (MODE (LONGLONG_BITS)));
A68_ENV_INT (genie_bytes_width, BYTES_WIDTH);
A68_ENV_INT (genie_long_bytes_width, LONG_BYTES_WIDTH);
A68_ENV_INT (genie_max_abs_char, UCHAR_MAX);
A68_ENV_INT (genie_max_int, MAX_INT);
A68_ENV_REAL (genie_max_real, DBL_MAX);
A68_ENV_REAL (genie_small_real, DBL_EPSILON);
A68_ENV_REAL (genie_pi, A68G_PI);
A68_ENV_REAL (genie_seconds, seconds ());
A68_ENV_REAL (genie_cputime, seconds () - cputime_0);
A68_ENV_INT (genie_stack_pointer, stack_pointer);
A68_ENV_INT (genie_system_stack_size, stack_size);

/*!
\brief INT system stack pointer
\param p position in syntax tree, should not be NULL position in syntax tree, should not be NULL
**/

void genie_system_stack_pointer (NODE_T * p)
{
  BYTE_T stack_offset;
  PUSH_INT (p, (int) system_stack_offset - (int) &stack_offset);
}

/*!
\brief LONG INT max long int
\param p position in syntax tree, should not be NULL
**/

void genie_long_max_int (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_DIGIT_T *z;
  int k, j = 1 + digits;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = digits - 1;
  for (k = 2; k <= j; k++) {
    z[k] = MP_RADIX - 1;
  }
}

/*!
\brief LONG LONG INT max long long int
\param p position in syntax tree, should not be NULL
**/

void genie_longlong_max_int (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_INT));
  MP_DIGIT_T *z;
  int k, j = 1 + digits;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = digits - 1;
  for (k = 2; k <= j; k++) {
    z[k] = MP_RADIX - 1;
  }
}

/*!
\brief LONG REAL max long real
\param p position in syntax tree, should not be NULL
**/

void genie_long_max_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = MAX_MP_EXPONENT - 1;
  for (j = 2; j <= 1 + digits; j++) {
    z[j] = MP_RADIX - 1;
  }
}

/*!
\brief LONG LONG REAL max long long real
\param p position in syntax tree, should not be NULL
**/

void genie_longlong_max_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = MAX_MP_EXPONENT - 1;
  for (j = 2; j <= 1 + digits; j++) {
    z[j] = MP_RADIX - 1;
  }
}

/*!
\brief LONG REAL small long real
\param p position in syntax tree, should not be NULL
**/

void genie_long_small_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = -(digits - 1);
  MP_DIGIT (z, 1) = 1;
  for (j = 3; j <= 1 + digits; j++) {
    z[j] = 0;
  }
}

/*!
\brief LONG LONG REAL small long long real
\param p position in syntax tree, should not be NULL
**/

void genie_longlong_small_real (NODE_T * p)
{
  int j, digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = -(digits - 1);
  MP_DIGIT (z, 1) = 1;
  for (j = 3; j <= 1 + digits; j++) {
    z[j] = 0;
  }
}

/*!
\brief BITS max bits
\param p position in syntax tree, should not be NULL
**/

void genie_max_bits (NODE_T * p)
{
  PUSH_BITS (p, MAX_BITS);
}

/*!
\brief LONG BITS long max bits
\param p position in syntax tree, should not be NULL
**/

void genie_long_max_bits (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_BITS));
  int width = get_mp_bits_width (MODE (LONG_BITS));
  ADDR_T pop_sp;
  MP_DIGIT_T *z, *one;
  STACK_MP (z, p, digits);
  pop_sp = stack_pointer;
  STACK_MP (one, p, digits);
  set_mp_short (z, (MP_DIGIT_T) 2, 0, digits);
  set_mp_short (one, (MP_DIGIT_T) 1, 0, digits);
  pow_mp_int (p, z, z, width, digits);
  sub_mp (p, z, z, one, digits);
  stack_pointer = pop_sp;
}

/*!
\brief LONG LONG BITS long long max bits
\param p position in syntax tree, should not be NULL
**/

void genie_longlong_max_bits (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_BITS));
  int width = get_mp_bits_width (MODE (LONGLONG_BITS));
  ADDR_T pop_sp;
  MP_DIGIT_T *z, *one;
  STACK_MP (z, p, digits);
  pop_sp = stack_pointer;
  STACK_MP (one, p, digits);
  set_mp_short (z, (MP_DIGIT_T) 2, 0, digits);
  set_mp_short (one, (MP_DIGIT_T) 1, 0, digits);
  pow_mp_int (p, z, z, width, digits);
  sub_mp (p, z, z, one, digits);
  stack_pointer = pop_sp;
}

/*!
\brief LONG REAL long pi
\param p position in syntax tree, should not be NULL
**/

void genie_pi_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  mp_pi (p, z, MP_PI, digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/* BOOL operations. */

/* OP NOT = (BOOL) BOOL. */

A68_MONAD (genie_not_bool, p, A68_BOOL, !);

/*!
\brief OP ABS = (BOOL) INT
\param p position in syntax tree, should not be NULL
**/

void genie_abs_bool (NODE_T * p)
{
  A68_BOOL j;
  POP_BOOL (p, &j);
  PUSH_INT (p, (j.value ? 1 : 0));
}

#define A68_BOOL_DYAD(n, p, OP) void n (NODE_T * p) {\
  A68_BOOL *i, *j;\
  POP_OPERAND_ADDRESSES (p, i, j, A68_BOOL);\
  i->value = i->value OP j->value;\
}

A68_BOOL_DYAD (genie_and_bool, p, &);
A68_BOOL_DYAD (genie_or_bool, p, |);
A68_BOOL_DYAD (genie_xor_bool, p, ^);
A68_BOOL_DYAD (genie_eq_bool, p, ==);
A68_BOOL_DYAD (genie_ne_bool, p, !=);

/* INT operations. */

/* OP - = (INT) INT. */

A68_MONAD (genie_minus_int, p, A68_INT, -);

/*!
\brief OP ABS = (INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_abs_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  j->value = ABS (j->value);
}

/*!
\brief OP SIGN = (INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_sign_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  j->value = SIGN (j->value);
}

/*!
\brief OP ODD = (INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_odd_int (NODE_T * p)
{
  A68_INT j;
  POP_INT (p, &j);
  PUSH_BOOL (p, (j.value >= 0 ? j.value : -j.value) % 2 == 1);
}

/*!
\brief OP + = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_add_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_INT_ADDITION (p, i->value, j->value);
  i->value += j->value;
}

/*!
\brief OP - = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_sub_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_INT_ADDITION (p, i->value, -j->value);
  i->value -= j->value;
}

/*!
\brief OP * = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_mul_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_TIMES_OVERFLOW_INT (p, i->value, j->value);
  i->value *= j->value;
}

/*!
\brief OP OVER = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_over_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  if (j->value == 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  i->value /= j->value;
}

/*!
\brief OP MOD = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_mod_int (NODE_T * p)
{
  A68_INT *i, *j;
  int k;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  if (j->value == 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  k = i->value % j->value;
  if (k < 0) {
    k += (j->value >= 0 ? j->value : -j->value);
  }
  i->value = k;
}

/*!
\brief OP / = (INT, INT) REAL
\param p position in syntax tree, should not be NULL
\return
**/

void genie_div_int (NODE_T * p)
{
  A68_INT i, j;
  POP_INT (p, &j);
  POP_INT (p, &i);
  if (j.value == 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_REAL (p, (double) (i.value) / (double) (j.value));
}

/*!
\brief OP ** = (INT, INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_pow_int (NODE_T * p)
{
  A68_INT i, j;
  int expo, mult, prod;
  POP_INT (p, &j);
  if (j.value < 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_EXPONENT_INVALID, MODE (INT), j.value);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  POP_INT (p, &i);
  prod = 1;
  mult = i.value;
  expo = 1;
  while ((unsigned) expo <= (unsigned) (j.value)) {
    if (j.value & expo) {
      TEST_TIMES_OVERFLOW_INT (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= j.value) {
      TEST_TIMES_OVERFLOW_INT (p, mult, mult);
      mult *= mult;
    }
  }
  PUSH_INT (p, prod);
}

/* OP (INT, INT) BOOL. */

#define A68_CMP_INT(n, p, OP) void n (NODE_T * p) {\
  A68_INT i, j;\
  POP_INT (p, &j);\
  POP_INT (p, &i);\
  PUSH_BOOL (p, i.value OP j.value);\
  }

A68_CMP_INT (genie_eq_int, p, ==);
A68_CMP_INT (genie_ne_int, p, !=);
A68_CMP_INT (genie_lt_int, p, <);
A68_CMP_INT (genie_gt_int, p, >);
A68_CMP_INT (genie_le_int, p, <=);
A68_CMP_INT (genie_ge_int, p, >=);

/*!
\brief OP +:= = (REF INT, INT) REF INT
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_add_int);
}

/*!
\brief OP -:= = (REF INT, INT) REF INT
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_sub_int);
}

/*!
\brief OP *:= = (REF INT, INT) REF INT
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mul_int);
}

/*!
\brief OP %:= = (REF INT, INT) REF INT
\param p position in syntax tree, should not be NULL
**/

void genie_overab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_over_int);
}

/*!
\brief OP %*:= = (REF INT, INT) REF INT
\param p position in syntax tree, should not be NULL
**/

void genie_modab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mod_int);
}

/*!
\brief OP LENG = (INT) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_lengthen_int_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_DIGIT_T *z;
  A68_INT k;
  POP_INT (p, &k);
  STACK_MP (z, p, digits);
  int_to_mp (p, z, k.value, digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP LENG = (BITS) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_lengthen_unsigned_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_DIGIT_T *z;
  A68_BITS k;
  POP_BITS (p, &k);
  STACK_MP (z, p, digits);
  unsigned_to_mp (p, z, (unsigned) k.value, digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG INT) INT
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_long_mp_to_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  MP_STATUS (z) = INITIALISED_MASK;
  PUSH_INT (p, mp_to_int (p, z, digits));
}

/*!
\brief OP ODD = (LONG INT) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_odd_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  if (MP_EXPONENT (z) <= digits - 1) {
    PUSH_BOOL (p, (int) (z[(int) (2 + MP_EXPONENT (z))]) % 2 != 0);
  } else {
    PUSH_BOOL (p, A_FALSE);
  }
}

/*!
\brief test whether z is a LONG INT
\param p position in syntax tree, should not be NULL
\param z
\param m
**/

void test_long_int_range (NODE_T * p, MP_DIGIT_T * z, MOID_T * m)
{
  if (!check_mp_int (z, m)) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief OP + = (LONG INT, LONG INT) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_add_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  add_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP - = (LONG INT, LONG INT) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_minus_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  sub_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP * = (LONG INT, LONG INT) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_mul_long_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  mul_mp (p, x, x, y, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP ** = (LONG MODE, INT) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_pow_long_mp_int_int (NODE_T * p)
{
  MOID_T *m = LHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  A68_INT k;
  MP_DIGIT_T *x;
  POP_INT (p, &k);
  x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  pow_mp_int (p, x, x, k.value, digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief OP +:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_int);
}

/*!
\brief OP -:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_minus_long_int);
}

/*!
\brief OP *:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_int);
}

/* REAL operations. REAL math is in gsl.c. */

/* OP - = (REAL) REAL. */

A68_MONAD (genie_minus_real, p, A68_REAL, -);

/*!
\brief OP ABS = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_abs_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  x->value = ABS (x->value);
}

/*!
\brief OP NINT = (REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_nint_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  x->value = (x->value > 0.0 ? floor (x->value) : ceil (x->value));
}

/*!
\brief OP ROUND = (REAL) INT
\param p position in syntax tree, should not be NULL
**/

void genie_round_real (NODE_T * p)
{
  A68_REAL x;
  int j;
  POP_REAL (p, &x);
  if (x.value < -MAX_INT || x.value > MAX_INT) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (x.value > 0) {
    j = (int) (x.value + 0.5);
  } else {
    j = (int) (x.value - 0.5);
  }
  PUSH_INT (p, j);
}

/*!
\brief OP ENTIER = (REAL) INT
\param p position in syntax tree, should not be NULL
**/

void genie_entier_real (NODE_T * p)
{
  A68_REAL x;
  POP_REAL (p, &x);
  if (x.value < -MAX_INT || x.value > MAX_INT) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_INT (p, (int) floor (x.value));
}

/*!
\brief OP SIGN = (REAL) INT
\param p position in syntax tree, should not be NULL
**/

void genie_sign_real (NODE_T * p)
{
  A68_REAL x;
  POP_REAL (p, &x);
  PUSH_INT (p, SIGN (x.value));
}

/*!
\brief OP + = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_add_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  x->value += y->value;
  TEST_REAL_REPRESENTATION (p, x->value);
}

/*!
\brief OP - = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sub_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  x->value -= y->value;
  TEST_REAL_REPRESENTATION (p, x->value);
}

/*!
\brief OP * = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_mul_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  TEST_TIMES_OVERFLOW_REAL (p, x->value, y->value);
  x->value *= y->value;
  TEST_REAL_REPRESENTATION (p, x->value);
}

/*!
\brief OP / = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_div_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
#ifndef HAVE_IEEE_754
  if (y->value == 0.0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (REAL));
    exit_genie (p, A_RUNTIME_ERROR);
  }
#endif
  x->value /= y->value;
  TEST_REAL_REPRESENTATION (p, x->value);
}

/*!
\brief OP ** = (REAL, INT) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_pow_real_int (NODE_T * p)
{
  A68_INT j;
  A68_REAL x;
  int expo;
  double mult, prod;
  BOOL_T negative;
  POP_INT (p, &j);
  negative = j.value < 0;
  j.value = (j.value >= 0 ? j.value : -j.value);
  POP_REAL (p, &x);
  prod = 1;
  mult = x.value;
  expo = 1;
  while ((unsigned) expo <= (unsigned) (j.value)) {
    if (j.value & expo) {
      TEST_TIMES_OVERFLOW_REAL (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= j.value) {
      TEST_TIMES_OVERFLOW_REAL (p, mult, mult);
      mult *= mult;
    }
  }
  TEST_REAL_REPRESENTATION (p, prod);
  if (negative) {
    prod = 1.0 / prod;
  }
  PUSH_REAL (p, prod);
}

/*!
\brief OP ** = (REAL, REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_pow_real (NODE_T * p)
{
  A68_REAL x, y;
  double z;
  POP_REAL (p, &y);
  POP_REAL (p, &x);
  if (x.value <= 0.0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (REAL), &x);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  RESET_ERRNO;
  z = exp (y.value * log (x.value));
  math_rte (p, errno != 0, MODE (REAL), NULL);
  PUSH_REAL (p, z);
}

/* OP (REAL, REAL) BOOL. */

#define A68_CMP_REAL(n, p, OP) void n (NODE_T * p) {\
  A68_REAL i, j;\
  POP_REAL (p, &j);\
  POP_REAL (p, &i);\
  PUSH_BOOL (p, i.value OP j.value);\
  }

A68_CMP_REAL (genie_eq_real, p, ==);
A68_CMP_REAL (genie_ne_real, p, !=);
A68_CMP_REAL (genie_lt_real, p, <);
A68_CMP_REAL (genie_gt_real, p, >);
A68_CMP_REAL (genie_le_real, p, <=);
A68_CMP_REAL (genie_ge_real, p, >=);

/*!
\brief OP +:= = (REF REAL, REAL) REF REAL
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_add_real);
}

/*!
\brief OP -:= = (REF REAL, REAL) REF REAL
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_sub_real);
}

/*!
\brief OP *:= = (REF REAL, REAL) REF REAL
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_mul_real);
}

/*!
\brief OP /:= = (REF REAL, REAL) REF REAL
\param p position in syntax tree, should not be NULL
**/

void genie_divab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_div_real);
}

/*!
\brief OP LENG = (REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_lengthen_real_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  A68_REAL x;
  POP_REAL (p, &x);
  STACK_MP (z, p, digits);
  real_to_mp (p, z, x.value, digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG REAL) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_long_mp_to_real (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_REAL (p, mp_to_real (p, z, digits));
}

/*!
\brief OP ROUND = (LONG REAL) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_round_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *y;
  STACK_MP (y, p, digits);
  set_mp_short (y, (MP_DIGIT_T) (MP_RADIX / 2), -1, digits);
  if (MP_DIGIT (z, 1) >= 0) {
    add_mp (p, z, z, y, digits);
    trunc_mp (p, z, z, digits);
  } else {
    sub_mp (p, z, z, y, digits);
    trunc_mp (p, z, z, digits);
  }
  MP_STATUS (z) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief OP ENTIER = (LONG REAL) LONG INT
\param p position in syntax tree, should not be NULL
**/

void genie_entier_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (LHS_MODE (p)), size = get_mp_size (LHS_MODE (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (MP_DIGIT (z, 1) >= 0) {
    trunc_mp (p, z, z, digits);
  } else {
    MP_DIGIT_T *y;
    STACK_MP (y, p, digits);
    set_mp_short (y, (MP_DIGIT_T) 1, 0, digits);
    trunc_mp (p, z, z, digits);
    sub_mp (p, z, z, y, digits);
  }
  MP_STATUS (z) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long sqrt = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sqrt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (sqrt_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longsqrt");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long curt = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_curt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (curt_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longcurt");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long exp = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_exp_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  exp_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long ln = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_ln_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (ln_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longln");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long log = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
\return
**/

void genie_log_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (log_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longlog");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}


/*!
\brief PROC long sinh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sinh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  sinh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long cosh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_cosh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  cosh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long tanh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_tanh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  tanh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arcsinh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arcsinh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  asinh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arccosh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arccosh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  acosh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arctanh = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arctanh_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  atanh_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long sin = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_sin_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  sin_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long cos = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_cos_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  cos_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long tan = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_tan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (tan_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longtan");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arcsin = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_asin_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (asin_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longarcsin");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arccos = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_acos_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (acos_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longarcsin");
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arctan = (LONG REAL) LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_atan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  atan_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/* Arithmetic operations. */

/*!
\brief OP LENG = (LONG MODE) LONG LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_lengthen_long_mp_to_longlong_mp (NODE_T * p)
{
  MP_DIGIT_T *z;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
  STACK_MP (z, p, longlong_mp_digits ());
  lengthen_mp (p, z, longlong_mp_digits (), z, long_mp_digits ());
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_longlong_mp_to_long_mp (NODE_T * p)
{
  MP_DIGIT_T *z;
  MOID_T *m = SUB (MOID (p));
  DECREMENT_STACK_POINTER (p, (int) size_longlong_mp ());
  STACK_MP (z, p, long_mp_digits ());
  if (m == MODE (LONG_INT)) {
    if (MP_EXPONENT (z) > LONG_MP_DIGITS - 1) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m, NULL);
      exit_genie (p, A_RUNTIME_ERROR);
    }
  }
  shorten_mp (p, z, long_mp_digits (), z, longlong_mp_digits ());
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP - = (LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_minus_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
}

/*!
\brief OP ABS = (LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_abs_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
}

/*!
\brief OP SIGN = (LONG MODE) INT
\param p position in syntax tree, should not be NULL
**/

void genie_sign_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_INT (p, SIGN (MP_DIGIT (z, 1)));
}

/*!
\brief OP + = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_add_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  add_mp (p, x, x, y, digits);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP - = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_sub_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  sub_mp (p, x, x, y, digits);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP * = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_mul_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  mul_mp (p, x, x, y, digits);
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP / = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_div_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (div_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_REAL));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP % = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_over_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (over_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP %* = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_mod_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (mod_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (MP_DIGIT (x, 1) < 0) {
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    add_mp (p, x, x, y, digits);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP +:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_mp);
}

/*!
\brief OP -:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_mp);
}

/*!
\brief OP *:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_mp);
}

/*!
\brief OP /:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_divab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_mp);
}

/*!
\brief OP %:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_overab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_over_long_mp);
}

/*!
\brief OP %*:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_modab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mod_long_mp);
}

/* OP (LONG MODE, LONG MODE) BOOL. */

#define A68_CMP_LONG(n, p, OP) void n (NODE_T * p) {\
  MOID_T *mode = LHS_MODE (p);\
  int digits = get_mp_digits (mode), size = get_mp_size (mode);\
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);\
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);\
  sub_mp (p, x, x, y, digits);\
  DECREMENT_STACK_POINTER (p, 2 * size);\
  PUSH_BOOL (p, MP_DIGIT (x, 1) OP 0);\
}

A68_CMP_LONG (genie_eq_long_mp, p, ==);
A68_CMP_LONG (genie_ne_long_mp, p, !=);
A68_CMP_LONG (genie_lt_long_mp, p, <);
A68_CMP_LONG (genie_gt_long_mp, p, >);
A68_CMP_LONG (genie_le_long_mp, p, <=);
A68_CMP_LONG (genie_ge_long_mp, p, >=);

/*!
\brief OP ** = (LONG MODE, INT) LONG MODE
\param p position in syntax tree, should not be NULL
**/

void genie_pow_long_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  A68_INT k;
  MP_DIGIT_T *x;
  POP_INT (p, &k);
  x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  pow_mp_int (p, x, x, k.value, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief OP ** = (LONG MODE, LONG MODE) LONG MODE
\param p position in syntax tree, should not be NULL
\return
**/

void genie_pow_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  if (ln_mp (p, z, x, digits) == NULL) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, SYMBOL (p));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  mul_mp (p, z, y, z, digits);
  exp_mp (p, x, z, digits);
  stack_pointer = pop_sp - size;
  MP_STATUS (x) = INITIALISED_MASK;
}

/* Character operations. */

/* OP (CHAR, CHAR) BOOL. */

#define A68_CMP_CHAR(n, p, OP) void n (NODE_T * p) {\
  A68_CHAR i, j;\
  POP_CHAR (p, &j);\
  POP_CHAR (p, &i);\
  PUSH_BOOL (p, TO_UCHAR (i.value) OP TO_UCHAR (j.value));\
  }

A68_CMP_CHAR (genie_eq_char, p, ==);
A68_CMP_CHAR (genie_ne_char, p, !=);
A68_CMP_CHAR (genie_lt_char, p, <);
A68_CMP_CHAR (genie_gt_char, p, >);
A68_CMP_CHAR (genie_le_char, p, <=);
A68_CMP_CHAR (genie_ge_char, p, >=);

/*!
\brief OP ABS = (CHAR) INT
\param p position in syntax tree, should not be NULL
**/

void genie_abs_char (NODE_T * p)
{
  A68_CHAR i;
  POP_CHAR (p, &i);
  PUSH_INT (p, TO_UCHAR (i.value));
}

/*!
\brief OP REPR = (INT) CHAR
\param p position in syntax tree, should not be NULL
**/

void genie_repr_char (NODE_T * p)
{
  A68_INT k;
  POP_INT (p, &k);
  if (k.value < 0 || k.value > (int) UCHAR_MAX) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (CHAR));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  PUSH_CHAR (p, (char) (k.value));
}

/*!
\brief OP + = (CHAR, CHAR) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_add_char (NODE_T * p)
{
  A68_CHAR a, b;
  A68_REF c, d;
  A68_ARRAY *a_3;
  A68_TUPLE *t_3;
  BYTE_T *b_3;
/* right part. */
  POP_CHAR (p, &b);
  TEST_INIT (p, b, MODE (CHAR));
/* left part. */
  POP_CHAR (p, &a);
  TEST_INIT (p, a, MODE (CHAR));
/* sum. */
  c = heap_generator (p, MODE (STRING), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&c);
  d = heap_generator (p, MODE (STRING), 2 * SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&d);
  GET_DESCRIPTOR (a_3, t_3, &c);
  a_3->dimensions = 1;
  MOID (a_3) = MODE (CHAR);
  a_3->elem_size = SIZE_OF (A68_CHAR);
  a_3->slice_offset = 0;
  a_3->field_offset = 0;
  a_3->array = d;
  t_3->lower_bound = 1;
  t_3->upper_bound = 2;
  t_3->shift = t_3->lower_bound;
  t_3->span = 1;
/* add chars. */
  b_3 = ADDRESS (&a_3->array);
  MOVE ((BYTE_T *) & b_3[0], (BYTE_T *) & a, SIZE_OF (A68_CHAR));
  MOVE ((BYTE_T *) & b_3[SIZE_OF (A68_CHAR)], (BYTE_T *) & b, SIZE_OF (A68_CHAR));
  PUSH_REF (p, c);
  UNPROTECT_SWEEP_HANDLE (&c);
  UNPROTECT_SWEEP_HANDLE (&d);
}

/*!
\brief OP ELEM = (INT, STRING) CHAR # ALGOL68C #
\param p position in syntax tree, should not be NULL
**/

void genie_elem_string (NODE_T * p)
{
  A68_REF z;
  A68_ARRAY *a;
  A68_TUPLE *t;
  A68_INT k;
  BYTE_T *base;
  A68_CHAR *ch;
  POP_REF (p, &z);
  TEST_INIT (p, z, MODE (STRING));
  TEST_NIL (p, z, MODE (STRING));
  POP_INT (p, &k);
  GET_DESCRIPTOR (a, t, &z);
  if (k.value < t->lower_bound) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  } else if (k.value > t->upper_bound) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  base = ADDRESS (&(a->array));
  ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, k.value)]);
  PUSH_CHAR (p, ch->value);
}

/*!
\brief OP + = (STRING, STRING) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_add_string (NODE_T * p)
{
  A68_REF a, b, c, d;
  A68_ARRAY *a_1, *a_2, *a_3;
  A68_TUPLE *t_1, *t_2, *t_3;
  int l_1, l_2, k, m;
  BYTE_T *b_1, *b_2, *b_3;
/* right part. */
  POP_REF (p, &b);
  TEST_INIT (p, b, MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &b);
  l_2 = ROW_SIZE (t_2);
/* left part. */
  POP_REF (p, &a);
  TEST_INIT (p, a, MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &a);
  l_1 = ROW_SIZE (t_1);
/* sum. */
  c = heap_generator (p, MODE (STRING), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&c);
  d = heap_generator (p, MODE (STRING), (l_1 + l_2) * SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&d);
/* Calculate again since heap sweeper might have moved data. */
  GET_DESCRIPTOR (a_1, t_1, &a);
  GET_DESCRIPTOR (a_2, t_2, &b);
  GET_DESCRIPTOR (a_3, t_3, &c);
  a_3->dimensions = 1;
  MOID (a_3) = MODE (CHAR);
  a_3->elem_size = SIZE_OF (A68_CHAR);
  a_3->slice_offset = 0;
  a_3->field_offset = 0;
  a_3->array = d;
  t_3->lower_bound = 1;
  t_3->upper_bound = l_1 + l_2;
  t_3->shift = t_3->lower_bound;
  t_3->span = 1;
/* add strings. */
  b_1 = ADDRESS (&a_1->array);
  b_2 = ADDRESS (&a_2->array);
  b_3 = ADDRESS (&a_3->array);
  m = 0;
  for (k = t_1->lower_bound; k <= t_1->upper_bound; k++) {
    MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, k)], SIZE_OF (A68_CHAR));
    m += SIZE_OF (A68_CHAR);
  }
  for (k = t_2->lower_bound; k <= t_2->upper_bound; k++) {
    MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_2[INDEX_1_DIM (a_2, t_2, k)], SIZE_OF (A68_CHAR));
    m += SIZE_OF (A68_CHAR);
  }
  PUSH_REF (p, c);
  UNPROTECT_SWEEP_HANDLE (&c);
  UNPROTECT_SWEEP_HANDLE (&d);
}

/*!
\brief OP * = (INT, STRING) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_times_int_string (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_REF (p, &a);
  POP_INT (p, &k);
  if (k.value < 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), k);
    exit_genie (p, A_RUNTIME_ERROR);
  }
  UP_SWEEP_SEMA;
  PUSH_REF (p, empty_string (p));
  while (k.value--) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
  DOWN_SWEEP_SEMA;
}

/*!
\brief OP * = (STRING, INT) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_times_string_int (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_INT (p, &k);
  POP_REF (p, &a);
  PUSH_INT (p, k.value);
  PUSH_REF (p, a);
  genie_times_int_string (p);
}

/*!
\brief OP * = (INT, CHAR) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_times_int_char (NODE_T * p)
{
  A68_INT str_size;
  A68_CHAR a;
  A68_REF z, row;
  A68_ARRAY arr;
  A68_TUPLE tup;
  BYTE_T *base;
  int k;
/* Pop operands. */
  POP_CHAR (p, &a);
  POP_INT (p, &str_size);
  if (str_size.value < 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), str_size);
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Make new_one string. */
  z = heap_generator (p, MODE (ROW_CHAR), SIZE_OF (A68_ARRAY) + SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&z);
  row = heap_generator (p, MODE (ROW_CHAR), (int) (str_size.value) * SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&row);
  arr.dimensions = 1;
  arr.type = MODE (CHAR);
  arr.elem_size = SIZE_OF (A68_CHAR);
  arr.slice_offset = 0;
  arr.field_offset = 0;
  arr.array = row;
  tup.lower_bound = 1;
  tup.upper_bound = str_size.value;
  tup.shift = tup.lower_bound;
  tup.span = 1;
  PUT_DESCRIPTOR (arr, tup, &z);
/* Copy. */
  base = ADDRESS (&row);
  for (k = 0; k < str_size.value; k++) {
    A68_CHAR ch;
    ch.status = INITIALISED_MASK;
    ch.value = a.value;
    *(A68_CHAR *) & base[k * SIZE_OF (A68_CHAR)] = ch;
  }
  PUSH_REF (p, z);
  UNPROTECT_SWEEP_HANDLE (&z);
  UNPROTECT_SWEEP_HANDLE (&row);
}

/*!
\brief OP * = (CHAR, INT) STRING
\param p position in syntax tree, should not be NULL
**/

void genie_times_char_int (NODE_T * p)
{
  A68_INT k;
  A68_CHAR a;
  POP_INT (p, &k);
  POP_CHAR (p, &a);
  PUSH_INT (p, k.value);
  PUSH_CHAR (p, a.value);
  genie_times_int_char (p);
}

/*!
\brief OP +:= = (REF STRING, STRING) REF STRING
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_string (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_STRING), genie_add_string);
}

/*!
\brief OP +=: = (STRING, REF STRING) REF STRING
\param p position in syntax tree, should not be NULL
**/

void genie_plusto_string (NODE_T * p)
{
  A68_REF refa, a, b;
  POP_REF (p, &refa);
  TEST_NIL (p, refa, MODE (REF_STRING));
  a = *(A68_REF *) ADDRESS (&refa);
  TEST_INIT (p, a, MODE (STRING));
  POP_REF (p, &b);
  PUSH_REF (p, b);
  PUSH_REF (p, a);
  genie_add_string (p);
  POP (p, (A68_REF *) ADDRESS (&refa), SIZE_OF (A68_REF));
  PUSH_REF (p, refa);
}

/*!
\brief OP *:= = (REF STRING, INT) REF STRING
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_string (NODE_T * p)
{
  A68_INT k;
  A68_REF refa, a;
  int i;
/* INT. */
  POP_INT (p, &k);
  if (k.value < 0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), k);
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* REF STRING. */
  POP_REF (p, &refa);
  TEST_NIL (p, refa, MODE (REF_STRING));
  a = *(A68_REF *) ADDRESS (&refa);
  TEST_INIT (p, a, MODE (STRING));
/* Multiplication as repeated addition. */
  PUSH_REF (p, empty_string (p));
  for (i = 1; i <= k.value; i++) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
/* The stack contains a STRING, promote to REF STRING. */
  POP_REF (p, (A68_REF *) ADDRESS (&refa));
  PUSH_REF (p, refa);
}

/*!
\brief difference between two STRINGs in the stack
\param p position in syntax tree, should not be NULL
\return -1 if a < b, 0 if a = b or -1 if a > b
**/

static int string_difference (NODE_T * p)
{
  A68_REF row1, row2;
  A68_ARRAY *a_1, *a_2;
  A68_TUPLE *t_1, *t_2;
  BYTE_T *b_1, *b_2;
  int size, s_1, s_2, k, diff;
/* Pop operands. */
  POP_REF (p, &row2);
  TEST_INIT (p, row2, MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &row2);
  s_2 = ROW_SIZE (t_2);
  POP_REF (p, &row1);
  TEST_INIT (p, row1, MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &row1);
  s_1 = ROW_SIZE (t_1);
/* Get difference. */
  size = s_1 > s_2 ? s_1 : s_2;
  diff = 0;
  b_1 = ADDRESS (&a_1->array);
  b_2 = ADDRESS (&a_2->array);
  for (k = 0; k < size && diff == 0; k++) {
    int a, b;
    if (s_1 > 0 && k < s_1) {
      A68_CHAR *ch = (A68_CHAR *) & b_1[INDEX_1_DIM (a_1, t_1, t_1->lower_bound + k)];
      a = (int) ch->value;
    } else {
      a = 0;
    }
    if (s_2 > 0 && k < s_2) {
      A68_CHAR *ch = (A68_CHAR *) & b_2[INDEX_1_DIM (a_2, t_2, t_2->lower_bound + k)];
      b = (int) ch->value;
    } else {
      b = 0;
    }
    diff += (TO_UCHAR (a) - TO_UCHAR (b));
  }
  return (diff);
}

/* OP (STRING, STRING) BOOL. */

#define A68_CMP_STRING(n, p, OP) void n (NODE_T * p) {\
  int k = string_difference (p);\
  PUSH_BOOL (p, k OP 0);\
}

A68_CMP_STRING (genie_eq_string, p, ==);
A68_CMP_STRING (genie_ne_string, p, !=);
A68_CMP_STRING (genie_lt_string, p, <);
A68_CMP_STRING (genie_gt_string, p, >);
A68_CMP_STRING (genie_le_string, p, <=);
A68_CMP_STRING (genie_ge_string, p, >=);

/* RNG functions are in gsl.c.*/

/*!
\brief PROC first random = (INT) VOID
\param p position in syntax tree, should not be NULL
**/

void genie_first_random (NODE_T * p)
{
  A68_INT i;
  POP_INT (p, &i);
  init_rng ((unsigned long) i.value);
}

/*!
\brief PROC next random = REAL
\param p position in syntax tree, should not be NULL
**/

void genie_next_random (NODE_T * p)
{
  PUSH_REAL (p, rng_53_bit ());
}

/*!
\brief PROC next long random = LONG REAL
\param p position in syntax tree, should not be NULL
**/

void genie_long_next_random (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p));
  MP_DIGIT_T *z;
  int k = 2 + digits;
  STACK_MP (z, p, digits);
  while (--k > 1) {
    z[k] = (int) (rng_53_bit () * MP_RADIX);
  }
  MP_EXPONENT (z) = -1;
  MP_STATUS (z) = INITIALISED_MASK;
}

/* BYTES operations. */

/*!
\brief OP ELEM = (INT, BYTES) CHAR
\param p position in syntax tree, should not be NULL
**/

void genie_elem_bytes (NODE_T * p)
{
  A68_BYTES j;
  A68_INT i;
  POP_BYTES (p, &j);
  POP_INT (p, &i);
  if (i.value < 1 || i.value > BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (i.value > (int) strlen (j.value)) {
    genie_null_char (p);
  } else {
    PUSH_CHAR (p, j.value[i.value - 1]);
  }
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_BYTES b;
  POP_REF (p, &z);
  TEST_INIT (p, z, MODE (STRING));
  TEST_NIL (p, z, MODE (STRING));
  if (a68_string_size (p, z) > BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  b.status = INITIALISED_MASK;
  a_to_c_string (p, b.value, z);
  PUSH (p, &b, SIZE_OF (A68_BYTES));
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_add_bytes (NODE_T * p)
{
  A68_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BYTES);
  if (((int) strlen (i->value) + (int) strlen (j->value)) > BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  bufcat (i->value, j->value, BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF BYTES, BYTES) REF BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_BYTES), genie_add_bytes);
}

/*!
\brief OP +=: = (BYTES, REF BYTES) REF BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_plusto_bytes (NODE_T * p)
{
  A68_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  TEST_NIL (p, z, MODE (REF_BYTES));
  address = (A68_BYTES *) ADDRESS (&z);
  TEST_INIT (p, *address, MODE (BYTES));
  POP (p, &i, SIZE_OF (A68_BYTES));
  if (((int) strlen (address->value) + (int) strlen (i.value)) > BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  bufcpy (j.value, i.value, BYTES_WIDTH);
  bufcat (j.value, address->value, BYTES_WIDTH);
  bufcpy (address->value, j.value, BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between BYTE strings
\param p position in syntax tree, should not be NULL
\return
**/

static int compare_bytes (NODE_T * p)
{
  A68_BYTES x, y;
  POP_BYTES (p, &y);
  POP_BYTES (p, &x);
  return (strcmp (x.value, y.value));
}

/* OP (BYTES, BYTES) BOOL. */

#define A68_CMP_BYTES(n, p, OP) void n (NODE_T * p) {\
  int k = compare_bytes (p);\
  PUSH_BOOL (p, k OP 0);\
}

A68_CMP_BYTES (genie_eq_bytes, p, ==);
A68_CMP_BYTES (genie_ne_bytes, p, !=);
A68_CMP_BYTES (genie_lt_bytes, p, <);
A68_CMP_BYTES (genie_gt_bytes, p, >);
A68_CMP_BYTES (genie_le_bytes, p, <=);
A68_CMP_BYTES (genie_ge_bytes, p, >=);

/*!
\brief OP LENG = (BYTES) LONG BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_leng_bytes (NODE_T * p)
{
  A68_BYTES a;
  POP_BYTES (p, &a);
  PUSH_LONG_BYTES (p, a.value);
}

/*!
\brief OP SHORTEN = (LONG BYTES) BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_bytes (NODE_T * p)
{
  A68_LONG_BYTES a;
  POP_LONG_BYTES (p, &a);
  PUSH_BYTES (p, a.value);
}

/*!
\brief OP ELEM = (INT, LONG BYTES) CHAR
\param p position in syntax tree, should not be NULL
**/

void genie_elem_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES j;
  A68_INT i;
  POP_LONG_BYTES (p, &j);
  POP_INT (p, &i);
  if (i.value < 1 || i.value > LONG_BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  if (i.value > (int) strlen (j.value)) {
    genie_null_char (p);
  } else {
    PUSH_CHAR (p, j.value[i.value - 1]);
  }
}

/*!
\brief PROC long bytes pack = (STRING) LONG BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_long_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_LONG_BYTES b;
  POP_REF (p, &z);
  TEST_INIT (p, z, MODE (STRING));
  TEST_NIL (p, z, MODE (STRING));
  if (a68_string_size (p, z) > LONG_BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  b.status = INITIALISED_MASK;
  a_to_c_string (p, b.value, z);
  PUSH (p, &b, SIZE_OF (A68_LONG_BYTES));
}

/*!
\brief OP + = (LONG BYTES, LONG BYTES) LONG BYTES
\param p position in syntax tree, should not be NULL
\return
**/

void genie_add_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_LONG_BYTES);
  if (((int) strlen (i->value) + (int) strlen (j->value)) > LONG_BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  bufcat (i->value, j->value, LONG_BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF LONG BYTES, LONG BYTES) REF LONG BYTES
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_long_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_LONG_BYTES), genie_add_long_bytes);
}

/*!
\brief OP +=: = (LONG BYTES, REF LONG BYTES) REF LONG BYTES
\param p position in syntax tree, should not be NULL
\return
**/

void genie_plusto_long_bytes (NODE_T * p)
{
  A68_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  TEST_NIL (p, z, MODE (REF_LONG_BYTES));
  address = (A68_BYTES *) ADDRESS (&z);
  TEST_INIT (p, *address, MODE (LONG_BYTES));
  POP (p, &i, SIZE_OF (A68_LONG_BYTES));
  if (((int) strlen (address->value) + (int) strlen (i.value)) > LONG_BYTES_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  bufcpy (j.value, i.value, LONG_BYTES_WIDTH);
  bufcat (j.value, address->value, LONG_BYTES_WIDTH);
  bufcpy (address->value, j.value, LONG_BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between LONG BYTE strings
\param p position in syntax tree, should not be NULL
\return
**/

static int compare_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES x, y;
  POP_LONG_BYTES (p, &y);
  POP_LONG_BYTES (p, &x);
  return (strcmp (x.value, y.value));
}

/* OP (LONG BYTES, LONG BYTES) BOOL. */

#define A68_CMP_LONG_BYTES(n, p, OP) void n (NODE_T * p) {\
  int k = compare_long_bytes (p);\
  PUSH_BOOL (p, k OP 0);\
}

A68_CMP_LONG_BYTES (genie_eq_long_bytes, p, ==);
A68_CMP_LONG_BYTES (genie_ne_long_bytes, p, !=);
A68_CMP_LONG_BYTES (genie_lt_long_bytes, p, <);
A68_CMP_LONG_BYTES (genie_gt_long_bytes, p, >);
A68_CMP_LONG_BYTES (genie_le_long_bytes, p, <=);
A68_CMP_LONG_BYTES (genie_ge_long_bytes, p, >=);

/* BITS operations. */

/* OP NOT = (BITS) BITS. */

A68_MONAD (genie_not_bits, p, A68_BITS, ~);

/*!
\brief OP AND = (BITS, BITS) BITS
\param p position in syntax tree, should not be NULL
\return
**/

void genie_and_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  i->value = i->value & j->value;
}

/*!
\brief OP OR = (BITS, BITS) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_or_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  i->value = i->value | j->value;
}

/*!
\brief OP XOR = (BITS, BITS) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_xor_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  i->value = i->value ^ j->value;
}

/* OP = = (BITS, BITS) BOOL. */

#define A68_CMP_BITS(n, p, OP) void n (NODE_T * p) {\
  A68_BITS i, j;\
  POP_BITS (p, &j);\
  POP_BITS (p, &i);\
  PUSH_BOOL (p, i.value OP j.value);\
  }

A68_CMP_BITS (genie_eq_bits, p, ==);
A68_CMP_BITS (genie_ne_bits, p, !=);
A68_CMP_BITS (genie_lt_bits, p, <);
A68_CMP_BITS (genie_gt_bits, p, >);
A68_CMP_BITS (genie_le_bits, p, <=);
A68_CMP_BITS (genie_ge_bits, p, >=);

/*!
\brief OP SHL = (BITS, INT) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_shl_bits (NODE_T * p)
{
  A68_BITS i;
  A68_INT j;
  POP_INT (p, &j);
  POP_BITS (p, &i);
  if (j.value >= 0) {
    if (i.value > (MAX_BITS >> j.value)) {
      diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BITS));
      exit_genie (p, A_RUNTIME_ERROR);
    }
    PUSH_BITS (p, i.value << j.value);
  } else {
    PUSH_BITS (p, i.value >> -j.value);
  }
}

/*!
\brief OP SHR = (BITS, INT) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_shr_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  j->value = -j->value;
  genie_shl_bits (p);           /* Conform RR. */
}

/*!
\brief OP ELEM = (INT, BITS) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_elem_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  POP_BITS (p, &j);
  POP_INT (p, &i);
  if (i.value < 1 || i.value > BITS_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  for (n = 0; n < (BITS_WIDTH - i.value); n++) {
    j.value = j.value >> 1;
  }
  PUSH_BOOL (p, j.value & 0x1);
}

/*!
\brief OP BIN = (INT) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_bin_int (NODE_T * p)
{
  A68_INT i;
  POP_INT (p, &i);
/* RR does not convert negative numbers. Algol68G does. */
  PUSH_BITS (p, i.value);
}

/*!
\brief OP BIN = (LONG INT) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_bin_long_mp (NODE_T * p)
{
  MOID_T *mode = SUB (MOID (p));
  int size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-size);
/* We convert just for the operand check. */
  (void) stack_mp_bits (p, u, mode);
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief OP NOT = (LONG BITS) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_not_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  int k, words = get_mp_bits_words (mode);
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row = stack_mp_bits (p, u, mode);
  for (k = 0; k < words; k++) {
    row[k] = ~row[k];
  }
  pack_mp_bits (p, u, row, mode);
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief OP SHORTEN = (LONG BITS) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_long_mp_to_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_BITS (p, mp_to_unsigned (p, z, digits));
}

/*!
\brief get bit from LONG BITS
\param p position in syntax tree, should not be NULL
\param k
\param z
\param m
\return
**/

unsigned elem_long_bits (NODE_T * p, int k, MP_DIGIT_T * z, MOID_T * m)
{
  int n;
  ADDR_T pop_sp = stack_pointer;
  unsigned *words = stack_mp_bits (p, z, m), w;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  w = words[k / MP_BITS_BITS];
  for (n = 0; n < (MP_BITS_BITS - k % MP_BITS_BITS - 1); n++) {
    w = w >> 1;
  }
  stack_pointer = pop_sp;
  return (w & 0x1);
}

/*!
\brief OP ELEM = (INT, LONG BITS) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_elem_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE_OF (A68_INT)));
  if (i->value < 1 || i->value > bits) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  w = elem_long_bits (p, i->value, z, MODE (LONG_BITS));
  DECREMENT_STACK_POINTER (p, size + SIZE_OF (A68_INT));
  PUSH_BOOL (p, w != 0);
}

/*!
\brief OP ELEM = (INT, LONG LONG BITS) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_elem_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + SIZE_OF (A68_INT)));
  if (i->value < 1 || i->value > bits) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A_RUNTIME_ERROR);
  }
  w = elem_long_bits (p, i->value, z, MODE (LONGLONG_BITS));
  DECREMENT_STACK_POINTER (p, size + SIZE_OF (A68_INT));
  PUSH_BOOL (p, w != 0);
}

/*!
\brief PROC bits pack = ([] BOOL) BITS
\param p position in syntax tree, should not be NULL
**/

void genie_bits_pack (NODE_T * p)
{
  A68_REF z;
  A68_BITS b;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k;
  unsigned bit;
  POP_REF (p, &z);
  TEST_INIT (p, z, MODE (ROW_BOOL));
  TEST_NIL (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  if (size < 0 || size > BITS_WIDTH) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Convert so that LWB goes to MSB, so ELEM gives same order. */
  base = ADDRESS (&arr->array);
  b.value = 0;
/* Set bit mask. */
  bit = 0x1;
  for (k = 0; k < BITS_WIDTH - size; k++) {
    bit <<= 1;
  }
  for (k = tup->upper_bound; k >= tup->lower_bound; k--) {
    int addr = INDEX_1_DIM (arr, tup, k);
    A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
    TEST_INIT (p, *boo, MODE (BOOL));
    if (boo->value) {
      b.value |= bit;
    }
    bit <<= 1;
  }
  b.status = INITIALISED_MASK;
  PUSH (p, &b, SIZE_OF (A68_BITS));
}

/*!
\brief PROC long bits pack = ([] BOOL) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_long_bits_pack (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  A68_REF z;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k, bits, digits;
  ADDR_T pop_sp;
  MP_DIGIT_T *sum, *fact;
  POP_REF (p, &z);
  TEST_INIT (p, z, MODE (ROW_BOOL));
  TEST_NIL (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  bits = get_mp_bits_width (mode);
  digits = get_mp_digits (mode);
  if (size < 0 || size > bits) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
    exit_genie (p, A_RUNTIME_ERROR);
  }
/* Convert so that LWB goes to MSB, so ELEM gives same order as [] BOOL. */
  base = ADDRESS (&arr->array);
  STACK_MP (sum, p, digits);
  SET_MP_ZERO (sum, digits);
  pop_sp = stack_pointer;
/* Set bit mask. */
  STACK_MP (fact, p, digits);
  set_mp_short (fact, (MP_DIGIT_T) 1, 0, digits);
  for (k = 0; k < bits - size; k++) {
    mul_mp_digit (p, fact, fact, (MP_DIGIT_T) 2, digits);
  }
  for (k = tup->upper_bound; k >= tup->lower_bound; k--) {
    int addr = INDEX_1_DIM (arr, tup, k);
    A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
    TEST_INIT (p, *boo, MODE (BOOL));
    if (boo->value) {
      add_mp (p, sum, sum, fact, digits);
    }
    mul_mp_digit (p, fact, fact, (MP_DIGIT_T) 2, digits);
  }
  stack_pointer = pop_sp;
  MP_STATUS (sum) = INITIALISED_MASK;
}

/*!
\brief OP SHL = (LONG BITS, INT) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_shl_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  MOID_T *int_m = (mode == MODE (LONG_BITS) ? MODE (LONG_INT) : MODE (LONGLONG_INT));
  int size = get_mp_size (mode), digits = get_mp_digits (mode);
  A68_INT j;
  ADDR_T pop_sp;
  MP_DIGIT_T *u, *two, *pow;
  BOOL_T multiply;
/* Pop number of bits. */
  POP_INT (p, &j);
  if (j.value >= 0) {
    multiply = A_TRUE;
  } else {
    multiply = A_FALSE;
    j.value = -j.value;
  }
  u = (MP_DIGIT_T *) STACK_OFFSET (-size);
/* Determine multiplication factor, 2 ** j. */
  pop_sp = stack_pointer;
  STACK_MP (two, p, digits);
  set_mp_short (two, (MP_DIGIT_T) 2, 0, digits);
  STACK_MP (pow, p, digits);
  pow_mp_int (p, pow, two, j.value, digits);
  test_long_int_range (p, pow, int_m);
/*Implement shift. */
  if (multiply) {
    mul_mp (p, u, u, pow, digits);
    check_long_bits_value (p, u, mode);
  } else {
    over_mp (p, u, u, pow, digits);
  }
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief OP SHR = (LONG BITS, INT) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_shr_long_mp (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  j->value = -j->value;
  genie_shl_long_mp (p);        /* Conform RR. */
}

/*!
\brief OP AND = (LONG BITS, LONG BITS) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_and_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-2 * size), *v = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] &= row_v[k];
  }
  pack_mp_bits (p, u, row_u, mode);
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP OR = (LONG BITS, LONG BITS) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_or_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-2 * size), *v = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] |= row_v[k];
  }
  pack_mp_bits (p, u, row_u, mode);
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP XOR = (LONG BITS, LONG BITS) LONG BITS
\param p position in syntax tree, should not be NULL
**/

void genie_xor_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-2 * size), *v = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; k < words; k++) {
    row_u[k] ^= row_v[k];
  }
  pack_mp_bits (p, u, row_u, mode);
  MP_STATUS (u) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}
