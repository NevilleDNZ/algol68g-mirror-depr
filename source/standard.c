/*!
\file standard.c
\brief standard prelude implementation, except transput
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

/*
This file contains Algol68G's standard environ. Transput routines are not here.
Some of the LONG operations are generic for LONG and LONG LONG.
This file contains calculus related routines from the C library and GNU
scientific library. When GNU scientific library is not installed then the
routines in this file will give a runtime error when called. You can also choose
to not have them defined in "prelude.c".
*/

#include "algol68g.h"
#include "genie.h"
#include "inline.h"
#include "mp.h"
#include "gsl.h"

#if defined ENABLE_NUMERICAL
#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_const.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf.h>
#endif

double inverf (double);
double inverfc (double);

double cputime_0;

#define A68_MONAD(n, MODE, OP)\
void n (NODE_T * p) {\
  MODE *i;\
  POP_OPERAND_ADDRESS (p, i, MODE);\
  VALUE (i) = OP VALUE (i);\
}

/*!
\brief math_rte math runtime error
\param p position in tree
\param z condition
\param m mode associated with error
\param t info text
**/

void math_rte (NODE_T * p, BOOL_T z, MOID_T * m, const char *t)
{
  if (z) {
    if (t == NULL) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_MATH, m, NULL);
    } else {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_MATH_INFO, m, t);
    }
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief generic procedure for OP AND BECOMES (+:=, -:=, ...)
\param p position in tree
\param ref mode of destination
\param f pointer to function that performs operation
**/

void genie_f_and_becomes (NODE_T * p, MOID_T * ref, GENIE_PROCEDURE * f)
{
  MOID_T *mode = SUB (ref);
  int size = MOID_SIZE (mode);
  BYTE_T *src = STACK_OFFSET (-size), *addr;
  A68_REF *dst = (A68_REF *) STACK_OFFSET (-size - ALIGNED_SIZE_OF (A68_REF));
  CHECK_REF (p, *dst, ref);
  addr = ADDRESS (dst);
  PUSH (p, addr, size);
  CHECK_INIT_GENERIC (p, STACK_OFFSET (-size), mode);
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
A68_ENV_INT (genie_max_int, A68_MAX_INT);
A68_ENV_REAL (genie_max_real, DBL_MAX);
A68_ENV_REAL (genie_min_real, DBL_MIN);
A68_ENV_REAL (genie_small_real, DBL_EPSILON);
A68_ENV_REAL (genie_pi, A68G_PI);
A68_ENV_REAL (genie_seconds, seconds ());
A68_ENV_REAL (genie_cputime, seconds () - cputime_0);
A68_ENV_INT (genie_stack_pointer, stack_pointer);
A68_ENV_INT (genie_system_stack_size, stack_size);

/*!
\brief INT system stack pointer
\param p position in tree
**/

void genie_system_stack_pointer (NODE_T * p)
{
  BYTE_T stack_offset;
  PUSH_PRIMITIVE (p, (int) system_stack_offset - (int) &stack_offset, A68_INT);
}

/*!
\brief LONG INT max long int
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\brief LONG REAL min long real
\param p position in tree
**/

void genie_long_min_real (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  SET_MP_ZERO (z, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = -(MAX_MP_EXPONENT);
  MP_DIGIT (z, 1) = 1.0;
}

/*!
\brief LONG LONG REAL min long long real
\param p position in tree
**/

void genie_longlong_min_real (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONGLONG_REAL));
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  SET_MP_ZERO (z, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  MP_EXPONENT (z) = -(MAX_MP_EXPONENT);
  MP_DIGIT (z, 1) = 1.0;
}

/*!
\brief LONG REAL small long real
\param p position in tree
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
\param p position in tree
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
\param p position in tree
**/

void genie_max_bits (NODE_T * p)
{
  PUSH_PRIMITIVE (p, A68_MAX_BITS, A68_BITS);
}

/*!
\brief LONG BITS long max bits
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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

A68_MONAD (genie_not_bool, A68_BOOL, !);

/*!
\brief OP ABS = (BOOL) INT
\param p position in tree
**/

void genie_abs_bool (NODE_T * p)
{
  A68_BOOL j;
  POP_OBJECT (p, &j, A68_BOOL);
  PUSH_PRIMITIVE (p, (VALUE (&j) ? 1 : 0), A68_INT);
}

#define A68_BOOL_DYAD(n, OP)\
void n (NODE_T * p) {\
  A68_BOOL *i, *j;\
  POP_OPERAND_ADDRESSES (p, i, j, A68_BOOL);\
  VALUE (i) = VALUE (i) OP VALUE (j);\
}

A68_BOOL_DYAD (genie_and_bool, &);
A68_BOOL_DYAD (genie_or_bool, |);
A68_BOOL_DYAD (genie_xor_bool, ^);
A68_BOOL_DYAD (genie_eq_bool, ==);
A68_BOOL_DYAD (genie_ne_bool, !=);

/* INT operations. */
/* OP - = (INT) INT. */

A68_MONAD (genie_minus_int, A68_INT, -);

/*!
\brief OP ABS = (INT) INT
\param p position in tree
**/
void genie_abs_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = ABS (VALUE (j));
}

/*!
\brief OP SIGN = (INT) INT
\param p position in tree
**/

void genie_sign_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = SIGN (VALUE (j));
}

/*!
\brief OP ODD = (INT) INT
\param p position in tree
**/

void genie_odd_int (NODE_T * p)
{
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  PUSH_PRIMITIVE (p, (VALUE (&j) >= 0 ? VALUE (&j) : -VALUE (&j)) % 2 == 1, A68_BOOL);
}

/*!
\brief OP + = (INT, INT) INT
\param p position in tree
**/

void genie_add_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_INT_ADDITION (p, VALUE (i), VALUE (j));
  VALUE (i) += VALUE (j);
}

/*!
\brief OP - = (INT, INT) INT
\param p position in tree
**/

void genie_sub_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_INT_ADDITION (p, VALUE (i), -VALUE (j));
  VALUE (i) -= VALUE (j);
}

/*!
\brief OP * = (INT, INT) INT
\param p position in tree
**/

void genie_mul_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  TEST_INT_MULTIPLICATION (p, VALUE (i), VALUE (j));
  VALUE (i) *= VALUE (j);
}

/*!
\brief OP OVER = (INT, INT) INT
\param p position in tree
**/

void genie_over_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  if (VALUE (j) == 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  VALUE (i) /= VALUE (j);
}

/*!
\brief OP MOD = (INT, INT) INT
\param p position in tree
**/

void genie_mod_int (NODE_T * p)
{
  A68_INT *i, *j;
  int k;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  if (VALUE (j) == 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  k = VALUE (i) % VALUE (j);
  if (k < 0) {
    k += (VALUE (j) >= 0 ? VALUE (j) : -VALUE (j));
  }
  VALUE (i) = k;
}

/*!
\brief OP / = (INT, INT) REAL
\param p position in tree
**/

void genie_div_int (NODE_T * p)
{
  A68_INT i, j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&j) == 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, (double) (VALUE (&i)) / (double) (VALUE (&j)), A68_REAL);
}

/*!
\brief OP ** = (INT, INT) INT
\param p position in tree
**/

void genie_pow_int (NODE_T * p)
{
  A68_INT i, j;
  int expo, mult, prod;
  POP_OBJECT (p, &j, A68_INT);
  if (VALUE (&j) < 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_EXPONENT_INVALID, MODE (INT), VALUE (&j));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  POP_OBJECT (p, &i, A68_INT);
  prod = 1;
  mult = VALUE (&i);
  expo = 1;
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (VALUE (&j) & expo) {
      TEST_INT_MULTIPLICATION (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= VALUE (&j)) {
      TEST_INT_MULTIPLICATION (p, mult, mult);
      mult *= mult;
    }
  }
  PUSH_PRIMITIVE (p, prod, A68_INT);
}

/* OP (INT, INT) BOOL. */

#define A68_CMP_INT(n, OP)\
void n (NODE_T * p) {\
  A68_INT i, j;\
  POP_OBJECT (p, &j, A68_INT);\
  POP_OBJECT (p, &i, A68_INT);\
  PUSH_PRIMITIVE (p, VALUE (&i) OP VALUE (&j), A68_BOOL);\
  }

A68_CMP_INT (genie_eq_int, ==);
A68_CMP_INT (genie_ne_int, !=);
A68_CMP_INT (genie_lt_int, <);
A68_CMP_INT (genie_gt_int, >);
A68_CMP_INT (genie_le_int, <=);
A68_CMP_INT (genie_ge_int, >=);

/*!
\brief OP +:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_plusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_add_int);
}

/*!
\brief OP -:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_minusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_sub_int);
}

/*!
\brief OP *:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_timesab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mul_int);
}

/*!
\brief OP %:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_overab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_over_int);
}

/*!
\brief OP %*:= = (REF INT, INT) REF INT
\param p position in tree
**/

void genie_modab_int (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_INT), genie_mod_int);
}

/*!
\brief OP LENG = (INT) LONG INT
\param p position in tree
**/

void genie_lengthen_int_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_DIGIT_T *z;
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  STACK_MP (z, p, digits);
  int_to_mp (p, z, VALUE (&k), digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP LENG = (BITS) LONG BITS
\param p position in tree
**/

void genie_lengthen_unsigned_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_INT));
  MP_DIGIT_T *z;
  A68_BITS k;
  POP_OBJECT (p, &k, A68_BITS);
  STACK_MP (z, p, digits);
  unsigned_to_mp (p, z, (unsigned) VALUE (&k), digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG INT) INT
\param p position in tree
**/

void genie_shorten_long_mp_to_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_DIGIT_T *) STACK_TOP;
  MP_STATUS (z) = INITIALISED_MASK;
  PUSH_PRIMITIVE (p, mp_to_int (p, z, digits), A68_INT);
}

/*!
\brief OP ODD = (LONG INT) BOOL
\param p position in tree
**/

void genie_odd_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  if (MP_EXPONENT (z) <= digits - 1) {
    PUSH_PRIMITIVE (p, (int) (z[(int) (2 + MP_EXPONENT (z))]) % 2 != 0, A68_BOOL);
  } else {
    PUSH_PRIMITIVE (p, A68_FALSE, A68_BOOL);
  }
}

/*!
\brief test whether z is a valid LONG INT
\param p position in tree
\param z mp number
\param m mode associated with z
**/

void test_long_int_range (NODE_T * p, MP_DIGIT_T * z, MOID_T * m)
{
  if (!check_mp_int (z, m)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief OP + = (LONG INT, LONG INT) LONG INT
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
**/

void genie_pow_long_mp_int_int (NODE_T * p)
{
  MOID_T *m = LHS_MODE (p);
  int digits = get_mp_digits (m), size = get_mp_size (m);
  A68_INT k;
  MP_DIGIT_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  pow_mp_int (p, x, x, VALUE (&k), digits);
  test_long_int_range (p, x, m);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief OP +:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_plusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_int);
}

/*!
\brief OP -:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_minusab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_minus_long_int);
}

/*!
\brief OP *:= = (REF LONG INT, LONG INT) REF LONG INT
\param p position in tree
**/

void genie_timesab_long_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_int);
}

/* REAL operations. REAL math is in gsl.c. */

/* OP - = (REAL) REAL. */

A68_MONAD (genie_minus_real, A68_REAL, -);

/*!
\brief OP ABS = (REAL) REAL
\param p position in tree
**/

void genie_abs_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  VALUE (x) = ABS (VALUE (x));
}

/*!
\brief OP ROUND = (REAL) INT
\param p position in tree
**/

void genie_round_real (NODE_T * p)
{
  A68_REAL x;
  int j;
  POP_OBJECT (p, &x, A68_REAL);
  if (VALUE (&x) < -A68_MAX_INT || VALUE (&x) > A68_MAX_INT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (VALUE (&x) > 0) {
    j = (int) (VALUE (&x) + 0.5);
  } else {
    j = (int) (VALUE (&x) - 0.5);
  }
  PUSH_PRIMITIVE (p, j, A68_INT);
}

/*!
\brief OP ENTIER = (REAL) INT
\param p position in tree
**/

void genie_entier_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  if (VALUE (&x) < -A68_MAX_INT || VALUE (&x) > A68_MAX_INT) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, (int) floor (VALUE (&x)), A68_INT);
}

/*!
\brief OP SIGN = (REAL) INT
\param p position in tree
**/

void genie_sign_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PUSH_PRIMITIVE (p, SIGN (VALUE (&x)), A68_INT);
}

/*!
\brief OP + = (REAL, REAL) REAL
\param p position in tree
**/

void genie_add_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) += VALUE (y);
  TEST_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP - = (REAL, REAL) REAL
\param p position in tree
**/

void genie_sub_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) -= VALUE (y);
  TEST_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP * = (REAL, REAL) REAL
\param p position in tree
**/

void genie_mul_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  TEST_TIMES_OVERFLOW_REAL (p, VALUE (x), VALUE (y));
  VALUE (x) *= VALUE (y);
  TEST_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP / = (REAL, REAL) REAL
\param p position in tree
**/

void genie_div_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
#if ! defined ENABLE_IEEE_754
  if (VALUE (y) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (REAL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
#endif
  VALUE (x) /= VALUE (y);
  TEST_REAL_REPRESENTATION (p, VALUE (x));
}

/*!
\brief OP ** = (REAL, INT) REAL
\param p position in tree
**/

void genie_pow_real_int (NODE_T * p)
{
  A68_INT j;
  A68_REAL x;
  int expo;
  double mult, prod;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  negative = VALUE (&j) < 0;
  VALUE (&j) = (VALUE (&j) >= 0 ? VALUE (&j) : -VALUE (&j));
  POP_OBJECT (p, &x, A68_REAL);
  prod = 1;
  mult = VALUE (&x);
  expo = 1;
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (VALUE (&j) & expo) {
      TEST_TIMES_OVERFLOW_REAL (p, prod, mult);
      prod *= mult;
    }
    expo <<= 1;
    if (expo <= VALUE (&j)) {
      TEST_TIMES_OVERFLOW_REAL (p, mult, mult);
      mult *= mult;
    }
  }
  TEST_REAL_REPRESENTATION (p, prod);
  if (negative) {
    prod = 1.0 / prod;
  }
  PUSH_PRIMITIVE (p, prod, A68_REAL);
}

/*!
\brief OP ** = (REAL, REAL) REAL
\param p position in tree
**/

void genie_pow_real (NODE_T * p)
{
  A68_REAL x, y;
  double z;
  POP_OBJECT (p, &y, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  if (VALUE (&x) <= 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (REAL), &x);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  RESET_ERRNO;
  z = exp (VALUE (&y) * log (VALUE (&x)));
  math_rte (p, errno != 0, MODE (REAL), NULL);
  PUSH_PRIMITIVE (p, z, A68_REAL);
}

/* OP (REAL, REAL) BOOL. */

#define A68_CMP_REAL(n, OP)\
void n (NODE_T * p) {\
  A68_REAL i, j;\
  POP_OBJECT (p, &j, A68_REAL);\
  POP_OBJECT (p, &i, A68_REAL);\
  PUSH_PRIMITIVE (p, VALUE (&i) OP VALUE (&j), A68_BOOL);\
  }

A68_CMP_REAL (genie_eq_real, ==);
A68_CMP_REAL (genie_ne_real, !=);
A68_CMP_REAL (genie_lt_real, <);
A68_CMP_REAL (genie_gt_real, >);
A68_CMP_REAL (genie_le_real, <=);
A68_CMP_REAL (genie_ge_real, >=);

/*!
\brief OP +:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_plusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_add_real);
}

/*!
\brief OP -:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_minusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_sub_real);
}

/*!
\brief OP *:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_timesab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_mul_real);
}

/*!
\brief OP /:= = (REF REAL, REAL) REF REAL
\param p position in tree
**/

void genie_divab_real (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_REAL), genie_div_real);
}

/*!
\brief OP LENG = (REAL) LONG REAL
\param p position in tree
**/

void genie_lengthen_real_to_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  STACK_MP (z, p, digits);
  real_to_mp (p, z, VALUE (&x), digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG REAL) REAL
\param p position in tree
**/

void genie_shorten_long_mp_to_real (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_DIGIT_T *) STACK_TOP;
  MP_STATUS (z) = INITIALISED_MASK;
  PUSH_PRIMITIVE (p, mp_to_real (p, z, digits), A68_REAL);
}

/*!
\brief OP ROUND = (LONG REAL) LONG INT
\param p position in tree
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
\param p position in tree
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
    MOVE_MP (y, z, digits);
    trunc_mp (p, z, z, digits);
    sub_mp (p, y, y, z, digits);
    if (MP_DIGIT (y, 1) != 0) {
      set_mp_short (y, (MP_DIGIT_T) 1, 0, digits);
      sub_mp (p, z, z, y, digits);
    }
  }
  MP_STATUS (z) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long sqrt = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_sqrt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (sqrt_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longsqrt");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long curt = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_curt_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (curt_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longcurt");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long exp = (LONG REAL) LONG REAL
\param p position in tree
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
\param p position in tree
**/

void genie_ln_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (ln_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longln");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}

/*!
\brief PROC long log = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_log_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (log_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longlog");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  stack_pointer = pop_sp;
}


/*!
\brief PROC long sinh = (LONG REAL) LONG REAL
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
**/

void genie_tan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (tan_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longtan");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arcsin = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_asin_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (asin_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longarcsin");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arccos = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_acos_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (acos_mp (p, x, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longarcsin");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arctan = (LONG REAL) LONG REAL
\param p position in tree
**/

void genie_atan_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  atan_mp (p, x, x, digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief PROC long arctan2 = (LONG REAL, LONG REAL) LONG REAL
\param p position in tree
**/

void genie_atan2_long_mp (NODE_T * p)
{
  int digits = get_mp_digits (MOID (p)), size = get_mp_size (MOID (p));
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  stack_pointer -= size;
  if (atan2_mp (p, x, y, x, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, "longarctan2");
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
}

/* Arithmetic operations. */

/*!
\brief OP LENG = (LONG MODE) LONG LONG MODE
\param p position in tree
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
\param p position in tree
**/

void genie_shorten_longlong_mp_to_long_mp (NODE_T * p)
{
  MP_DIGIT_T *z;
  MOID_T *m = SUB (MOID (p));
  DECREMENT_STACK_POINTER (p, (int) size_longlong_mp ());
  STACK_MP (z, p, long_mp_digits ());
  if (m == MODE (LONG_INT)) {
    if (MP_EXPONENT (z) > LONG_MP_DIGITS - 1) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, m, NULL);
      exit_genie (p, A68_RUNTIME_ERROR);
    }
  }
  shorten_mp (p, z, long_mp_digits (), z, longlong_mp_digits ());
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP - = (LONG MODE) LONG MODE
\param p position in tree
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
\param p position in tree
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
\param p position in tree
**/

void genie_sign_long_mp (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_PRIMITIVE (p, SIGN (MP_DIGIT (z, 1)), A68_INT);
}

/*!
\brief OP + = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
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
\param p position in tree
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
\param p position in tree
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
\param p position in tree
**/

void genie_div_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (div_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_REAL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP % = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_over_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (over_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_STATUS (x) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP %* = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
**/

void genie_mod_long_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);
  if (mod_mp (p, x, x, y, digits) == NULL) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (LONG_INT));
    exit_genie (p, A68_RUNTIME_ERROR);
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
\param p position in tree
**/

void genie_plusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_mp);
}

/*!
\brief OP -:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_minusab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_mp);
}

/*!
\brief OP *:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_timesab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_mp);
}

/*!
\brief OP /:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_divab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_mp);
}

/*!
\brief OP %:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_overab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_over_long_mp);
}

/*!
\brief OP %*:= = (REF LONG MODE, LONG MODE) REF LONG MODE
\param p position in tree
**/

void genie_modab_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mod_long_mp);
}

/* OP (LONG MODE, LONG MODE) BOOL. */

#define A68_CMP_LONG(n, OP)\
void n (NODE_T * p) {\
  MOID_T *mode = LHS_MODE (p);\
  int digits = get_mp_digits (mode), size = get_mp_size (mode);\
  MP_DIGIT_T *x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);\
  MP_DIGIT_T *y = (MP_DIGIT_T *) STACK_OFFSET (-size);\
  sub_mp (p, x, x, y, digits);\
  DECREMENT_STACK_POINTER (p, 2 * size);\
  PUSH_PRIMITIVE (p, MP_DIGIT (x, 1) OP 0, A68_BOOL);\
}

A68_CMP_LONG (genie_eq_long_mp, ==);
A68_CMP_LONG (genie_ne_long_mp, !=);
A68_CMP_LONG (genie_lt_long_mp, <);
A68_CMP_LONG (genie_gt_long_mp, >);
A68_CMP_LONG (genie_le_long_mp, <=);
A68_CMP_LONG (genie_ge_long_mp, >=);

/*!
\brief OP ** = (LONG MODE, INT) LONG MODE
\param p position in tree
**/

void genie_pow_long_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  A68_INT k;
  MP_DIGIT_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  pow_mp_int (p, x, x, VALUE (&k), digits);
  MP_STATUS (x) = INITIALISED_MASK;
}

/*!
\brief OP ** = (LONG MODE, LONG MODE) LONG MODE
\param p position in tree
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
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MOID (p), x, SYMBOL (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  mul_mp (p, z, y, z, digits);
  exp_mp (p, x, z, digits);
  stack_pointer = pop_sp - size;
  MP_STATUS (x) = INITIALISED_MASK;
}

/* Character operations. */

/* OP (CHAR, CHAR) BOOL. */

#define A68_CMP_CHAR(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR i, j;\
  POP_OBJECT (p, &j, A68_CHAR);\
  POP_OBJECT (p, &i, A68_CHAR);\
  PUSH_PRIMITIVE (p, TO_UCHAR (VALUE (&i)) OP TO_UCHAR (VALUE (&j)), A68_BOOL);\
  }

A68_CMP_CHAR (genie_eq_char, ==);
A68_CMP_CHAR (genie_ne_char, !=);
A68_CMP_CHAR (genie_lt_char, <);
A68_CMP_CHAR (genie_gt_char, >);
A68_CMP_CHAR (genie_le_char, <=);
A68_CMP_CHAR (genie_ge_char, >=);

/*!
\brief OP ABS = (CHAR) INT
\param p position in tree
**/

void genie_abs_char (NODE_T * p)
{
  A68_CHAR i;
  POP_OBJECT (p, &i, A68_CHAR);
  PUSH_PRIMITIVE (p, TO_UCHAR (VALUE (&i)), A68_INT);
}

/*!
\brief OP REPR = (INT) CHAR
\param p position in tree
**/

void genie_repr_char (NODE_T * p)
{
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  if (VALUE (&k) < 0 || VALUE (&k) > (int) UCHAR_MAX) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (CHAR));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_PRIMITIVE (p, (char) (VALUE (&k)), A68_CHAR);
}

/* OP (CHAR) BOOL. */

#define A68_CHAR_BOOL(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR ch;\
  POP_OBJECT (p, &ch, A68_CHAR);\
  PUSH_PRIMITIVE (p, (OP (VALUE (&ch)) == 0 ? A68_FALSE : A68_TRUE), A68_BOOL);\
  }

A68_CHAR_BOOL (genie_is_alnum, IS_ALNUM);
A68_CHAR_BOOL (genie_is_alpha, IS_ALPHA);
A68_CHAR_BOOL (genie_is_cntrl, IS_CNTRL);
A68_CHAR_BOOL (genie_is_digit, IS_DIGIT);
A68_CHAR_BOOL (genie_is_graph, IS_GRAPH);
A68_CHAR_BOOL (genie_is_lower, IS_LOWER);
A68_CHAR_BOOL (genie_is_print, IS_PRINT);
A68_CHAR_BOOL (genie_is_punct, IS_PUNCT);
A68_CHAR_BOOL (genie_is_space, IS_SPACE);
A68_CHAR_BOOL (genie_is_upper, IS_UPPER);
A68_CHAR_BOOL (genie_is_xdigit, IS_XDIGIT);

#define A68_CHAR_CHAR(n, OP)\
void n (NODE_T * p) {\
  A68_CHAR *ch;\
  POP_OPERAND_ADDRESS (p, ch, A68_CHAR);\
  VALUE (ch) = OP (TO_UCHAR (VALUE (ch)));\
}

A68_CHAR_CHAR (genie_to_lower, TO_LOWER);
A68_CHAR_CHAR (genie_to_upper, TO_UPPER);

/*!
\brief OP + = (CHAR, CHAR) STRING
\param p position in tree
**/

void genie_add_char (NODE_T * p)
{
  A68_CHAR a, b;
  A68_REF c, d;
  A68_ARRAY *a_3;
  A68_TUPLE *t_3;
  BYTE_T *b_3;
/* right part. */
  POP_OBJECT (p, &b, A68_CHAR);
  CHECK_INIT (p, INITIALISED (&b), MODE (CHAR));
/* left part. */
  POP_OBJECT (p, &a, A68_CHAR);
  CHECK_INIT (p, INITIALISED (&a), MODE (CHAR));
/* sum. */
  c = heap_generator (p, MODE (STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&c);
  d = heap_generator (p, MODE (STRING), 2 * ALIGNED_SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&d);
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = MODE (CHAR);
  a_3->elem_size = ALIGNED_SIZE_OF (A68_CHAR);
  a_3->slice_offset = 0;
  a_3->field_offset = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = 2;
  t_3->shift = LWB (t_3);
  t_3->span = 1;
/* add chars. */
  b_3 = ADDRESS (&ARRAY (a_3));
  MOVE ((BYTE_T *) & b_3[0], (BYTE_T *) & a, ALIGNED_SIZE_OF (A68_CHAR));
  MOVE ((BYTE_T *) & b_3[ALIGNED_SIZE_OF (A68_CHAR)], (BYTE_T *) & b, ALIGNED_SIZE_OF (A68_CHAR));
  PUSH_REF (p, c);
  UNPROTECT_SWEEP_HANDLE (&c);
  UNPROTECT_SWEEP_HANDLE (&d);
}

/*!
\brief OP ELEM = (INT, STRING) CHAR # ALGOL68C #
\param p position in tree
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
  CHECK_REF (p, z, MODE (STRING));
  POP_OBJECT (p, &k, A68_INT);
  GET_DESCRIPTOR (a, t, &z);
  if (VALUE (&k) < LWB (t)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  } else if (VALUE (&k) > UPB (t)) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INDEX_OUT_OF_BOUNDS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  base = ADDRESS (&(ARRAY (a)));
  ch = (A68_CHAR *) & (base[INDEX_1_DIM (a, t, VALUE (&k))]);
  PUSH_PRIMITIVE (p, VALUE (ch), A68_CHAR);
}

/*!
\brief OP + = (STRING, STRING) STRING
\param p position in tree
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
  CHECK_INIT (p, INITIALISED (&b), MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &b);
  l_2 = ROW_SIZE (t_2);
/* left part. */
  POP_REF (p, &a);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &a);
  l_1 = ROW_SIZE (t_1);
/* sum. */
  c = heap_generator (p, MODE (STRING), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&c);
  d = heap_generator (p, MODE (STRING), (l_1 + l_2) * ALIGNED_SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&d);
/* Calculate again since heap sweeper might have moved data. */
  GET_DESCRIPTOR (a_1, t_1, &a);
  GET_DESCRIPTOR (a_2, t_2, &b);
  GET_DESCRIPTOR (a_3, t_3, &c);
  DIM (a_3) = 1;
  MOID (a_3) = MODE (CHAR);
  a_3->elem_size = ALIGNED_SIZE_OF (A68_CHAR);
  a_3->slice_offset = 0;
  a_3->field_offset = 0;
  ARRAY (a_3) = d;
  LWB (t_3) = 1;
  UPB (t_3) = l_1 + l_2;
  t_3->shift = LWB (t_3);
  t_3->span = 1;
/* add strings. */
  b_1 = ADDRESS (&ARRAY (a_1));
  b_2 = ADDRESS (&ARRAY (a_2));
  b_3 = ADDRESS (&ARRAY (a_3));
  m = 0;
  for (k = LWB (t_1); k <= UPB (t_1); k++) {
    MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_1[INDEX_1_DIM (a_1, t_1, k)], ALIGNED_SIZE_OF (A68_CHAR));
    m += ALIGNED_SIZE_OF (A68_CHAR);
  }
  for (k = LWB (t_2); k <= UPB (t_2); k++) {
    MOVE ((BYTE_T *) & b_3[m], (BYTE_T *) & b_2[INDEX_1_DIM (a_2, t_2, k)], ALIGNED_SIZE_OF (A68_CHAR));
    m += ALIGNED_SIZE_OF (A68_CHAR);
  }
  PUSH_REF (p, c);
  UNPROTECT_SWEEP_HANDLE (&c);
  UNPROTECT_SWEEP_HANDLE (&d);
}

/*!
\brief OP * = (INT, STRING) STRING
\param p position in tree
**/

void genie_times_int_string (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_REF (p, &a);
  POP_OBJECT (p, &k, A68_INT);
  if (VALUE (&k) < 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), k);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  UP_SWEEP_SEMA;
  PUSH_REF (p, empty_string (p));
  while (VALUE (&k)--) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
  DOWN_SWEEP_SEMA;
}

/*!
\brief OP * = (STRING, INT) STRING
\param p position in tree
**/

void genie_times_string_int (NODE_T * p)
{
  A68_INT k;
  A68_REF a;
  POP_OBJECT (p, &k, A68_INT);
  POP_REF (p, &a);
  PUSH_PRIMITIVE (p, VALUE (&k), A68_INT);
  PUSH_REF (p, a);
  genie_times_int_string (p);
}

/*!
\brief OP * = (INT, CHAR) STRING
\param p position in tree
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
  POP_OBJECT (p, &a, A68_CHAR);
  POP_OBJECT (p, &str_size, A68_INT);
  if (VALUE (&str_size) < 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), str_size);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Make new_one string. */
  z = heap_generator (p, MODE (ROW_CHAR), ALIGNED_SIZE_OF (A68_ARRAY) + ALIGNED_SIZE_OF (A68_TUPLE));
  PROTECT_SWEEP_HANDLE (&z);
  row = heap_generator (p, MODE (ROW_CHAR), (int) (VALUE (&str_size)) * ALIGNED_SIZE_OF (A68_CHAR));
  PROTECT_SWEEP_HANDLE (&row);
  DIM (&arr) = 1;
  MOID (&arr) = MODE (CHAR);
  arr.elem_size = ALIGNED_SIZE_OF (A68_CHAR);
  arr.slice_offset = 0;
  arr.field_offset = 0;
  ARRAY (&arr) = row;
  LWB (&tup) = 1;
  UPB (&tup) = VALUE (&str_size);
  tup.shift = LWB (&tup);
  tup.span = 1;
  tup.k = 0;
  PUT_DESCRIPTOR (arr, tup, &z);
/* Copy. */
  base = ADDRESS (&row);
  for (k = 0; k < VALUE (&str_size); k++) {
    A68_CHAR ch;
    STATUS (&ch) = INITIALISED_MASK;
    VALUE (&ch) = VALUE (&a);
    *(A68_CHAR *) & base[k * ALIGNED_SIZE_OF (A68_CHAR)] = ch;
  }
  PUSH_REF (p, z);
  UNPROTECT_SWEEP_HANDLE (&z);
  UNPROTECT_SWEEP_HANDLE (&row);
}

/*!
\brief OP * = (CHAR, INT) STRING
\param p position in tree
**/

void genie_times_char_int (NODE_T * p)
{
  A68_INT k;
  A68_CHAR a;
  POP_OBJECT (p, &k, A68_INT);
  POP_OBJECT (p, &a, A68_CHAR);
  PUSH_PRIMITIVE (p, VALUE (&k), A68_INT);
  PUSH_PRIMITIVE (p, VALUE (&a), A68_CHAR);
  genie_times_int_char (p);
}

/*!
\brief OP +:= = (REF STRING, STRING) REF STRING
\param p position in tree
**/

void genie_plusab_string (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_STRING), genie_add_string);
}

/*!
\brief OP +=: = (STRING, REF STRING) REF STRING
\param p position in tree
**/

void genie_plusto_string (NODE_T * p)
{
  A68_REF refa, a, b;
  POP_REF (p, &refa);
  CHECK_REF (p, refa, MODE (REF_STRING));
  a = *(A68_REF *) ADDRESS (&refa);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
  POP_REF (p, &b);
  PUSH_REF (p, b);
  PUSH_REF (p, a);
  genie_add_string (p);
  POP_REF (p, (A68_REF *) ADDRESS (&refa));
  PUSH_REF (p, refa);
}

/*!
\brief OP *:= = (REF STRING, INT) REF STRING
\param p position in tree
**/

void genie_timesab_string (NODE_T * p)
{
  A68_INT k;
  A68_REF refa, a;
  int i;
/* INT. */
  POP_OBJECT (p, &k, A68_INT);
  if (VALUE (&k) < 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (INT), k);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* REF STRING. */
  POP_REF (p, &refa);
  CHECK_REF (p, refa, MODE (REF_STRING));
  a = *(A68_REF *) ADDRESS (&refa);
  CHECK_INIT (p, INITIALISED (&a), MODE (STRING));
/* Multiplication as repeated addition. */
  PUSH_REF (p, empty_string (p));
  for (i = 1; i <= VALUE (&k); i++) {
    PUSH_REF (p, a);
    genie_add_string (p);
  }
/* The stack contains a STRING, promote to REF STRING. */
  POP_REF (p, (A68_REF *) ADDRESS (&refa));
  PUSH_REF (p, refa);
}

/*!
\brief difference between two STRINGs in the stack
\param p position in tree
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
  CHECK_INIT (p, INITIALISED (&row2), MODE (STRING));
  GET_DESCRIPTOR (a_2, t_2, &row2);
  s_2 = ROW_SIZE (t_2);
  POP_REF (p, &row1);
  CHECK_INIT (p, INITIALISED (&row1), MODE (STRING));
  GET_DESCRIPTOR (a_1, t_1, &row1);
  s_1 = ROW_SIZE (t_1);
/* Get difference. */
  size = s_1 > s_2 ? s_1 : s_2;
  diff = 0;
  b_1 = ADDRESS (&ARRAY (a_1));
  b_2 = ADDRESS (&ARRAY (a_2));
  for (k = 0; k < size && diff == 0; k++) {
    int a, b;
    if (s_1 > 0 && k < s_1) {
      A68_CHAR *ch = (A68_CHAR *) & b_1[INDEX_1_DIM (a_1, t_1, LWB (t_1) + k)];
      a = (int) VALUE (ch);
    } else {
      a = 0;
    }
    if (s_2 > 0 && k < s_2) {
      A68_CHAR *ch = (A68_CHAR *) & b_2[INDEX_1_DIM (a_2, t_2, LWB (t_2) + k)];
      b = (int) VALUE (ch);
    } else {
      b = 0;
    }
    diff += (TO_UCHAR (a) - TO_UCHAR (b));
  }
  return (diff);
}

/* OP (STRING, STRING) BOOL. */

#define A68_CMP_STRING(n, OP)\
void n (NODE_T * p) {\
  int k = string_difference (p);\
  PUSH_PRIMITIVE (p, k OP 0, A68_BOOL);\
}

A68_CMP_STRING (genie_eq_string, ==);
A68_CMP_STRING (genie_ne_string, !=);
A68_CMP_STRING (genie_lt_string, <);
A68_CMP_STRING (genie_gt_string, >);
A68_CMP_STRING (genie_le_string, <=);
A68_CMP_STRING (genie_ge_string, >=);

/* RNG functions are in gsl.c.*/

/*!
\brief PROC first random = (INT) VOID
\param p position in tree
**/
void genie_first_random (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  init_rng ((unsigned long) VALUE (&i));
}

/*!
\brief PROC next random = REAL
\param p position in tree
**/

void genie_next_random (NODE_T * p)
{
  PUSH_PRIMITIVE (p, rng_53_bit (), A68_REAL);
}

/*!
\brief PROC next long random = LONG REAL
\param p position in tree
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
\param p position in tree
**/

void genie_elem_bytes (NODE_T * p)
{
  A68_BYTES j;
  A68_INT i;
  POP_OBJECT (p, &j, A68_BYTES);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 1 || VALUE (&i) > BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (VALUE (&i) > (int) strlen (VALUE (&j))) {
    genie_null_char (p);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&j)[VALUE (&i) - 1], A68_CHAR);
  }
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in tree
**/

void genie_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_BYTES b;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (STRING));
  if (a68_string_size (p, z) > BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  STATUS (&b) = INITIALISED_MASK;
  a_to_c_string (p, VALUE (&b), z);
  PUSH_BYTES (p, VALUE (&b));
}

/*!
\brief PROC bytes pack = (STRING) BYTES
\param p position in tree
**/

void genie_add_bytes (NODE_T * p)
{
  A68_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BYTES);
  if (((int) strlen (VALUE (i)) + (int) strlen (VALUE (j))) > BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  bufcat (VALUE (i), VALUE (j), BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF BYTES, BYTES) REF BYTES
\param p position in tree
**/

void genie_plusab_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_BYTES), genie_add_bytes);
}

/*!
\brief OP +=: = (BYTES, REF BYTES) REF BYTES
\param p position in tree
**/

void genie_plusto_bytes (NODE_T * p)
{
  A68_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (REF_BYTES));
  address = (A68_BYTES *) ADDRESS (&z);
  CHECK_INIT (p, INITIALISED (address), MODE (BYTES));
  POP_OBJECT (p, &i, A68_BYTES);
  if (((int) strlen (VALUE (address)) + (int) strlen (VALUE (&i))) > BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  bufcpy (VALUE (&j), VALUE (&i), BYTES_WIDTH);
  bufcat (VALUE (&j), VALUE (address), BYTES_WIDTH);
  bufcpy (VALUE (address), VALUE (&j), BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between BYTE strings
\param p position in tree
\return difference between objects
**/

static int compare_bytes (NODE_T * p)
{
  A68_BYTES x, y;
  POP_OBJECT (p, &y, A68_BYTES);
  POP_OBJECT (p, &x, A68_BYTES);
  return (strcmp (VALUE (&x), VALUE (&y)));
}

/* OP (BYTES, BYTES) BOOL. */

#define A68_CMP_BYTES(n, OP)\
void n (NODE_T * p) {\
  int k = compare_bytes (p);\
  PUSH_PRIMITIVE (p, k OP 0, A68_BOOL);\
}

A68_CMP_BYTES (genie_eq_bytes, ==);
A68_CMP_BYTES (genie_ne_bytes, !=);
A68_CMP_BYTES (genie_lt_bytes, <);
A68_CMP_BYTES (genie_gt_bytes, >);
A68_CMP_BYTES (genie_le_bytes, <=);
A68_CMP_BYTES (genie_ge_bytes, >=);

/*!
\brief OP LENG = (BYTES) LONG BYTES
\param p position in tree
**/

void genie_leng_bytes (NODE_T * p)
{
  A68_BYTES a;
  POP_OBJECT (p, &a, A68_BYTES);
  PUSH_LONG_BYTES (p, VALUE (&a));
}

/*!
\brief OP SHORTEN = (LONG BYTES) BYTES
\param p position in tree
**/

void genie_shorten_bytes (NODE_T * p)
{
  A68_LONG_BYTES a;
  POP_OBJECT (p, &a, A68_LONG_BYTES);
  if (strlen (VALUE (&a)) >= BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BYTES));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_BYTES (p, VALUE (&a));
}

/*!
\brief OP ELEM = (INT, LONG BYTES) CHAR
\param p position in tree
**/

void genie_elem_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES j;
  A68_INT i;
  POP_OBJECT (p, &j, A68_LONG_BYTES);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 1 || VALUE (&i) > LONG_BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  if (VALUE (&i) > (int) strlen (VALUE (&j))) {
    genie_null_char (p);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&j)[VALUE (&i) - 1], A68_CHAR);
  }
}

/*!
\brief PROC long bytes pack = (STRING) LONG BYTES
\param p position in tree
**/

void genie_long_bytespack (NODE_T * p)
{
  A68_REF z;
  A68_LONG_BYTES b;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (STRING));
  if (a68_string_size (p, z) > LONG_BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (STRING));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  STATUS (&b) = INITIALISED_MASK;
  a_to_c_string (p, VALUE (&b), z);
  PUSH_LONG_BYTES (p, VALUE (&b));
}

/*!
\brief OP + = (LONG BYTES, LONG BYTES) LONG BYTES
\param p position in tree
**/

void genie_add_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_LONG_BYTES);
  if (((int) strlen (VALUE (i)) + (int) strlen (VALUE (j))) > LONG_BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  bufcat (VALUE (i), VALUE (j), LONG_BYTES_WIDTH);
}

/*!
\brief OP +:= = (REF LONG BYTES, LONG BYTES) REF LONG BYTES
\param p position in tree
**/

void genie_plusab_long_bytes (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_LONG_BYTES), genie_add_long_bytes);
}

/*!
\brief OP +=: = (LONG BYTES, REF LONG BYTES) REF LONG BYTES
\param p position in tree
**/

void genie_plusto_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES i, *address, j;
  A68_REF z;
  POP_REF (p, &z);
  CHECK_REF (p, z, MODE (REF_LONG_BYTES));
  address = (A68_LONG_BYTES *) ADDRESS (&z);
  CHECK_INIT (p, INITIALISED (address), MODE (LONG_BYTES));
  POP_OBJECT (p, &i, A68_LONG_BYTES);
  if (((int) strlen (VALUE (address)) + (int) strlen (VALUE (&i))) > LONG_BYTES_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (LONG_BYTES));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  bufcpy (VALUE (&j), VALUE (&i), LONG_BYTES_WIDTH);
  bufcat (VALUE (&j), VALUE (address), LONG_BYTES_WIDTH);
  bufcpy (VALUE (address), VALUE (&j), LONG_BYTES_WIDTH);
  PUSH_REF (p, z);
}

/*!
\brief difference between LONG BYTE strings
\param p position in tree
\return difference between objects
**/

static int compare_long_bytes (NODE_T * p)
{
  A68_LONG_BYTES x, y;
  POP_OBJECT (p, &y, A68_LONG_BYTES);
  POP_OBJECT (p, &x, A68_LONG_BYTES);
  return (strcmp (VALUE (&x), VALUE (&y)));
}

/* OP (LONG BYTES, LONG BYTES) BOOL. */

#define A68_CMP_LONG_BYTES(n, OP)\
void n (NODE_T * p) {\
  int k = compare_long_bytes (p);\
  PUSH_PRIMITIVE (p, k OP 0, A68_BOOL);\
}

A68_CMP_LONG_BYTES (genie_eq_long_bytes, ==);
A68_CMP_LONG_BYTES (genie_ne_long_bytes, !=);
A68_CMP_LONG_BYTES (genie_lt_long_bytes, <);
A68_CMP_LONG_BYTES (genie_gt_long_bytes, >);
A68_CMP_LONG_BYTES (genie_le_long_bytes, <=);
A68_CMP_LONG_BYTES (genie_ge_long_bytes, >=);

/* BITS operations. */

/* OP NOT = (BITS) BITS. */

A68_MONAD (genie_not_bits, A68_BITS, ~);

/*!
\brief OP AND = (BITS, BITS) BITS
\param p position in tree
**/

void genie_and_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) & VALUE (j);
}

/*!
\brief OP OR = (BITS, BITS) BITS
\param p position in tree
**/

void genie_or_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) | VALUE (j);
}

/*!
\brief OP XOR = (BITS, BITS) BITS
\param p position in tree
**/

void genie_xor_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) ^ VALUE (j);
}

/* OP = = (BITS, BITS) BOOL. */

#define A68_CMP_BITS(n, OP)\
void n (NODE_T * p) {\
  A68_BITS i, j;\
  POP_OBJECT (p, &j, A68_BITS);\
  POP_OBJECT (p, &i, A68_BITS);\
  PUSH_PRIMITIVE (p, VALUE (&i) OP VALUE (&j), A68_BOOL);\
  }

A68_CMP_BITS (genie_eq_bits, ==);
A68_CMP_BITS (genie_ne_bits, !=);

/*!
\brief OP <= = (BITS, BITS) BOOL
\param p position in tree
**/

void genie_le_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_PRIMITIVE (p, (VALUE (&i) | VALUE (&j)) == VALUE (&j), A68_BOOL);
}

/*!
\brief OP >= = (BITS, BITS) BOOL
\param p position in tree
**/

void genie_ge_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_PRIMITIVE (p, (VALUE (&i) | VALUE (&j)) == VALUE (&i), A68_BOOL);
}

/*!
\brief OP SHL = (BITS, INT) BITS
\param p position in tree
**/

void genie_shl_bits (NODE_T * p)
{
  A68_BITS i;
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_BITS);
  if (VALUE (&j) >= 0) {
/*
    if (VALUE (&i) > (A68_MAX_BITS >> VALUE (&j))) {
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (BITS));
      exit_genie (p, A68_RUNTIME_ERROR);
    }
*/
    PUSH_PRIMITIVE (p, VALUE (&i) << VALUE (&j), A68_BITS);
  } else {
    PUSH_PRIMITIVE (p, VALUE (&i) >> -VALUE (&j), A68_BITS);
  }
}

/*!
\brief OP SHR = (BITS, INT) BITS
\param p position in tree
**/

void genie_shr_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_shl_bits (p);           /* Conform RR. */
}

/*!
\brief OP ELEM = (INT, BITS) BOOL
\param p position in tree
**/

void genie_elem_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, ((VALUE (&j) & mask) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP SET = (INT, BITS) BITS
\param p position in tree
**/

void genie_set_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, VALUE (&j) | mask, A68_BITS);
}

/*!
\brief OP CLEAR = (INT, BITS) BITS
\param p position in tree
**/

void genie_clear_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  unsigned mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  if (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_PRIMITIVE (p, VALUE (&j) & ~mask, A68_BITS);
}

/*!
\brief OP BIN = (INT) BITS
\param p position in tree
**/

void genie_bin_int (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
/* RR does not convert negative numbers. Algol68G does. */
  PUSH_PRIMITIVE (p, VALUE (&i), A68_BITS);
}

/*!
\brief OP BIN = (LONG INT) LONG BITS
\param p position in tree
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
\param p position in tree
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
  stack_pointer = pop_sp;
}

/*!
\brief OP SHORTEN = (LONG BITS) BITS
\param p position in tree
**/

void genie_shorten_long_mp_to_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  MP_DIGIT_T *z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_PRIMITIVE (p, mp_to_unsigned (p, z, digits), A68_BITS);
}

/*!
\brief get bit from LONG BITS
\param p position in tree
\param k element number
\param z mp number
\param m mode associated with z
\return same
**/

unsigned elem_long_bits (NODE_T * p, ADDR_T k, MP_DIGIT_T * z, MOID_T * m)
{
  int n;
  ADDR_T pop_sp = stack_pointer;
  unsigned *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  stack_pointer = pop_sp;
  return ((words[k / MP_BITS_BITS]) & mask);
}

/*!
\brief OP ELEM = (INT, LONG BITS) BOOL
\param p position in tree
**/

void genie_elem_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = elem_long_bits (p, VALUE (i), z, MODE (LONG_BITS));
  DECREMENT_STACK_POINTER (p, size + ALIGNED_SIZE_OF (A68_INT));
  PUSH_PRIMITIVE (p, w != 0, A68_BOOL);
}

/*!
\brief OP ELEM = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_elem_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned w;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = elem_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS));
  DECREMENT_STACK_POINTER (p, size + ALIGNED_SIZE_OF (A68_INT));
  PUSH_PRIMITIVE (p, w != 0, A68_BOOL);
}

/*!
\brief set bit in LONG BITS
\param p position in tree
\param k bit index
\param z mp number
\param m mode associated with z
**/

static unsigned *set_long_bits (NODE_T * p, int k, MP_DIGIT_T * z, MOID_T * m, unsigned bit)
{
  int n;
  unsigned *words = stack_mp_bits (p, z, m), mask = 0x1;
  k += (MP_BITS_BITS - get_mp_bits_width (m) % MP_BITS_BITS - 1);
  for (n = 0; n < MP_BITS_BITS - k % MP_BITS_BITS - 1; n++) {
    mask = mask << 1;
  }
  if (bit == 0x1) {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) | mask;
  } else {
    words[k / MP_BITS_BITS] = (words[k / MP_BITS_BITS]) & (~mask);
  }
  return (words);
}

/*!
\brief OP SET = (INT, LONG BITS) VOID
\param p position in tree
**/

void genie_set_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = set_long_bits (p, VALUE (i), z, MODE (LONG_BITS), 0x1);
  pack_mp_bits (p, (MP_DIGIT_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP SET = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_set_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = set_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS), 0x1);
  pack_mp_bits (p, (MP_DIGIT_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONGLONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP CLEAR = (INT, LONG BITS) BOOL
\param p position in tree
**/

void genie_clear_long_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONG_BITS)), size = get_mp_size (MODE (LONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = set_long_bits (p, VALUE (i), z, MODE (LONG_BITS), 0x0);
  pack_mp_bits (p, (MP_DIGIT_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief OP CLEAR = (INT, LONG LONG BITS) BOOL
\param p position in tree
**/

void genie_clear_longlong_bits (NODE_T * p)
{
  A68_INT *i;
  MP_DIGIT_T *z;
  unsigned *w;
  ADDR_T pop_sp = stack_pointer;
  int bits = get_mp_bits_width (MODE (LONGLONG_BITS)), size = get_mp_size (MODE (LONGLONG_BITS));
  z = (MP_DIGIT_T *) STACK_OFFSET (-size);
  i = (A68_INT *) STACK_OFFSET (-(size + ALIGNED_SIZE_OF (A68_INT)));
  if (VALUE (i) < 1 || VALUE (i) > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (INT));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  w = set_long_bits (p, VALUE (i), z, MODE (LONGLONG_BITS), 0x0);
  pack_mp_bits (p, (MP_DIGIT_T *) STACK_ADDRESS (pop_sp - size - ALIGNED_SIZE_OF (A68_INT)), w, MODE (LONGLONG_BITS));
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_INT));
}

/*!
\brief PROC bits pack = ([] BOOL) BITS
\param p position in tree
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
  CHECK_REF (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  if (size < 0 || size > BITS_WIDTH) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Convert so that LWB goes to MSB, so ELEM gives same order. */
  base = ADDRESS (&ARRAY (arr));
  VALUE (&b) = 0;
/* Set bit mask. */
  bit = 0x1;
  for (k = 0; k < BITS_WIDTH - size; k++) {
    bit <<= 1;
  }
  for (k = UPB (tup); k >= LWB (tup); k--) {
    int addr = INDEX_1_DIM (arr, tup, k);
    A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
    CHECK_INIT (p, INITIALISED (boo), MODE (BOOL));
    if (VALUE (boo)) {
      VALUE (&b) |= bit;
    }
    bit <<= 1;
  }
  STATUS (&b) = INITIALISED_MASK;
  PUSH_OBJECT (p, b, A68_BITS);
}

/*!
\brief PROC long bits pack = ([] BOOL) LONG BITS
\param p position in tree
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
  CHECK_REF (p, z, MODE (ROW_BOOL));
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  bits = get_mp_bits_width (mode);
  digits = get_mp_digits (mode);
  if (size < 0 || size > bits) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MODE (ROW_BOOL));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
/* Convert so that LWB goes to MSB, so ELEM gives same order as [] BOOL. */
  base = ADDRESS (&ARRAY (arr));
  STACK_MP (sum, p, digits);
  SET_MP_ZERO (sum, digits);
  pop_sp = stack_pointer;
/* Set bit mask. */
  STACK_MP (fact, p, digits);
  set_mp_short (fact, (MP_DIGIT_T) 1, 0, digits);
  for (k = 0; k < bits - size; k++) {
    mul_mp_digit (p, fact, fact, (MP_DIGIT_T) 2, digits);
  }
  for (k = UPB (tup); k >= LWB (tup); k--) {
    int addr = INDEX_1_DIM (arr, tup, k);
    A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
    CHECK_INIT (p, INITIALISED (boo), MODE (BOOL));
    if (VALUE (boo)) {
      add_mp (p, sum, sum, fact, digits);
    }
    mul_mp_digit (p, fact, fact, (MP_DIGIT_T) 2, digits);
  }
  stack_pointer = pop_sp;
  MP_STATUS (sum) = INITIALISED_MASK;
}

/*!
\brief OP SHL = (LONG BITS, INT) LONG BITS
\param p position in tree
**/

void genie_shl_long_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int i, k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  MP_DIGIT_T *u;
  unsigned *row_u;
  ADDR_T pop_sp;
  A68_INT j;
/* Pop number of bits. */
  POP_OBJECT (p, &j, A68_INT);
  u = (MP_DIGIT_T *) STACK_OFFSET (-size);
  pop_sp = stack_pointer;
  row_u = stack_mp_bits (p, u, mode);
  if (VALUE (&j) >= 0) {
    for (i = 0; i < VALUE (&j); i++) {
      BOOL_T carry = A68_FALSE;
      for (k = words - 1; k >= 0; k--) {
        row_u[k] <<= 1;
        if (carry) {
          row_u[k] |= 0x1;
        }
        carry = ((row_u[k] & MP_BITS_RADIX) != 0);
        row_u[k] &= ~(MP_BITS_RADIX);
      }
    }
  } else {
    for (i = 0; i < -VALUE (&j); i++) {
      BOOL_T carry = A68_FALSE;
      for (k = 0; k < words; k++) {
        if (carry) {
          row_u[k] |= MP_BITS_RADIX;
        }
        carry = ((row_u[k] & 0x1) != 0);
        row_u[k] >>= 1;
      }
    }
  }
  pack_mp_bits (p, u, row_u, mode);
  stack_pointer = pop_sp;
}

/*!
\brief OP SHR = (LONG BITS, INT) LONG BITS
\param p position in tree
**/

void genie_shr_long_mp (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_shl_long_mp (p);        /* Conform RR. */
}

/*!
\brief OP <= = (LONG BITS, LONG BITS) BOOL
\param p position in tree
**/

void genie_le_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  BOOL_T result = A68_TRUE;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-2 * size), *v = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result &= ((row_u[k] | row_v[k]) == row_v[k]);
  }
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP >= = (LONG BITS, LONG BITS) BOOL
\param p position in tree
**/

void genie_ge_long_bits (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int k, size = get_mp_size (mode), words = get_mp_bits_words (mode);
  ADDR_T pop_sp = stack_pointer;
  BOOL_T result = A68_TRUE;
  MP_DIGIT_T *u = (MP_DIGIT_T *) STACK_OFFSET (-2 * size), *v = (MP_DIGIT_T *) STACK_OFFSET (-size);
  unsigned *row_u = stack_mp_bits (p, u, mode), *row_v = stack_mp_bits (p, v, mode);
  for (k = 0; (k < words) && result; k++) {
    result &= ((row_u[k] | row_v[k]) == row_u[k]);
  }
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, (result ? A68_TRUE : A68_FALSE), A68_BOOL);
}

/*!
\brief OP AND = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
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
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP OR = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
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
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

/*!
\brief OP XOR = (LONG BITS, LONG BITS) LONG BITS
\param p position in tree
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
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
}

#if defined ENABLE_NUMERICAL
A68_ENV_REAL (genie_cgs_acre, GSL_CONST_CGS_ACRE);
A68_ENV_REAL (genie_cgs_angstrom, GSL_CONST_CGS_ANGSTROM);
A68_ENV_REAL (genie_cgs_astronomical_unit, GSL_CONST_CGS_ASTRONOMICAL_UNIT);
A68_ENV_REAL (genie_cgs_bar, GSL_CONST_CGS_BAR);
A68_ENV_REAL (genie_cgs_barn, GSL_CONST_CGS_BARN);
A68_ENV_REAL (genie_cgs_bohr_magneton, GSL_CONST_CGS_BOHR_MAGNETON);
A68_ENV_REAL (genie_cgs_bohr_radius, GSL_CONST_CGS_BOHR_RADIUS);
A68_ENV_REAL (genie_cgs_boltzmann, GSL_CONST_CGS_BOLTZMANN);
A68_ENV_REAL (genie_cgs_btu, GSL_CONST_CGS_BTU);
A68_ENV_REAL (genie_cgs_calorie, GSL_CONST_CGS_CALORIE);
A68_ENV_REAL (genie_cgs_canadian_gallon, GSL_CONST_CGS_CANADIAN_GALLON);
A68_ENV_REAL (genie_cgs_carat, GSL_CONST_CGS_CARAT);
A68_ENV_REAL (genie_cgs_cup, GSL_CONST_CGS_CUP);
A68_ENV_REAL (genie_cgs_curie, GSL_CONST_CGS_CURIE);
A68_ENV_REAL (genie_cgs_day, GSL_CONST_CGS_DAY);
A68_ENV_REAL (genie_cgs_dyne, GSL_CONST_CGS_DYNE);
A68_ENV_REAL (genie_cgs_electron_charge, GSL_CONST_CGS_ELECTRON_CHARGE);
A68_ENV_REAL (genie_cgs_electron_magnetic_moment, GSL_CONST_CGS_ELECTRON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_electron_volt, GSL_CONST_CGS_ELECTRON_VOLT) A68_ENV_REAL (genie_cgs_erg, GSL_CONST_CGS_ERG);
A68_ENV_REAL (genie_cgs_faraday, GSL_CONST_CGS_FARADAY) A68_ENV_REAL (genie_cgs_fathom, GSL_CONST_CGS_FATHOM);
A68_ENV_REAL (genie_cgs_fluid_ounce, GSL_CONST_CGS_FLUID_OUNCE) A68_ENV_REAL (genie_cgs_foot, GSL_CONST_CGS_FOOT);
A68_ENV_REAL (genie_cgs_footcandle, GSL_CONST_CGS_FOOTCANDLE) A68_ENV_REAL (genie_cgs_footlambert, GSL_CONST_CGS_FOOTLAMBERT);
A68_ENV_REAL (genie_cgs_gauss, GSL_CONST_CGS_GAUSS) A68_ENV_REAL (genie_cgs_gram_force, GSL_CONST_CGS_GRAM_FORCE);
A68_ENV_REAL (genie_cgs_grav_accel, GSL_CONST_CGS_GRAV_ACCEL);
A68_ENV_REAL (genie_cgs_gravitational_constant, GSL_CONST_CGS_GRAVITATIONAL_CONSTANT);
A68_ENV_REAL (genie_cgs_hectare, GSL_CONST_CGS_HECTARE) A68_ENV_REAL (genie_cgs_horsepower, GSL_CONST_CGS_HORSEPOWER);
A68_ENV_REAL (genie_cgs_hour, GSL_CONST_CGS_HOUR) A68_ENV_REAL (genie_cgs_inch, GSL_CONST_CGS_INCH);
A68_ENV_REAL (genie_cgs_inch_of_mercury, GSL_CONST_CGS_INCH_OF_MERCURY);
A68_ENV_REAL (genie_cgs_inch_of_water, GSL_CONST_CGS_INCH_OF_WATER) A68_ENV_REAL (genie_cgs_joule, GSL_CONST_CGS_JOULE);
A68_ENV_REAL (genie_cgs_kilometers_per_hour, GSL_CONST_CGS_KILOMETERS_PER_HOUR);
A68_ENV_REAL (genie_cgs_kilopound_force, GSL_CONST_CGS_KILOPOUND_FORCE) A68_ENV_REAL (genie_cgs_knot, GSL_CONST_CGS_KNOT);
A68_ENV_REAL (genie_cgs_lambert, GSL_CONST_CGS_LAMBERT) A68_ENV_REAL (genie_cgs_light_year, GSL_CONST_CGS_LIGHT_YEAR);
A68_ENV_REAL (genie_cgs_liter, GSL_CONST_CGS_LITER) A68_ENV_REAL (genie_cgs_lumen, GSL_CONST_CGS_LUMEN);
A68_ENV_REAL (genie_cgs_lux, GSL_CONST_CGS_LUX) A68_ENV_REAL (genie_cgs_mass_electron, GSL_CONST_CGS_MASS_ELECTRON);
A68_ENV_REAL (genie_cgs_mass_muon, GSL_CONST_CGS_MASS_MUON) A68_ENV_REAL (genie_cgs_mass_neutron, GSL_CONST_CGS_MASS_NEUTRON);
A68_ENV_REAL (genie_cgs_mass_proton, GSL_CONST_CGS_MASS_PROTON);
A68_ENV_REAL (genie_cgs_meter_of_mercury, GSL_CONST_CGS_METER_OF_MERCURY);
A68_ENV_REAL (genie_cgs_metric_ton, GSL_CONST_CGS_METRIC_TON) A68_ENV_REAL (genie_cgs_micron, GSL_CONST_CGS_MICRON);
A68_ENV_REAL (genie_cgs_mil, GSL_CONST_CGS_MIL) A68_ENV_REAL (genie_cgs_mile, GSL_CONST_CGS_MILE);
A68_ENV_REAL (genie_cgs_miles_per_hour, GSL_CONST_CGS_MILES_PER_HOUR) A68_ENV_REAL (genie_cgs_minute, GSL_CONST_CGS_MINUTE);
A68_ENV_REAL (genie_cgs_molar_gas, GSL_CONST_CGS_MOLAR_GAS) A68_ENV_REAL (genie_cgs_nautical_mile, GSL_CONST_CGS_NAUTICAL_MILE);
A68_ENV_REAL (genie_cgs_newton, GSL_CONST_CGS_NEWTON) A68_ENV_REAL (genie_cgs_nuclear_magneton, GSL_CONST_CGS_NUCLEAR_MAGNETON);
A68_ENV_REAL (genie_cgs_ounce_mass, GSL_CONST_CGS_OUNCE_MASS) A68_ENV_REAL (genie_cgs_parsec, GSL_CONST_CGS_PARSEC);
A68_ENV_REAL (genie_cgs_phot, GSL_CONST_CGS_PHOT) A68_ENV_REAL (genie_cgs_pint, GSL_CONST_CGS_PINT);
A68_ENV_REAL (genie_cgs_planck_constant_h, 6.6260693e-27) A68_ENV_REAL (genie_cgs_planck_constant_hbar, 6.6260693e-27 / (2 * A68G_PI));
A68_ENV_REAL (genie_cgs_point, GSL_CONST_CGS_POINT) A68_ENV_REAL (genie_cgs_poise, GSL_CONST_CGS_POISE);
A68_ENV_REAL (genie_cgs_pound_force, GSL_CONST_CGS_POUND_FORCE) A68_ENV_REAL (genie_cgs_pound_mass, GSL_CONST_CGS_POUND_MASS);
A68_ENV_REAL (genie_cgs_poundal, GSL_CONST_CGS_POUNDAL);
A68_ENV_REAL (genie_cgs_proton_magnetic_moment, GSL_CONST_CGS_PROTON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_cgs_psi, GSL_CONST_CGS_PSI) A68_ENV_REAL (genie_cgs_quart, GSL_CONST_CGS_QUART);
A68_ENV_REAL (genie_cgs_rad, GSL_CONST_CGS_RAD) A68_ENV_REAL (genie_cgs_roentgen, GSL_CONST_CGS_ROENTGEN);
A68_ENV_REAL (genie_cgs_rydberg, GSL_CONST_CGS_RYDBERG) A68_ENV_REAL (genie_cgs_solar_mass, GSL_CONST_CGS_SOLAR_MASS);
A68_ENV_REAL (genie_cgs_speed_of_light, GSL_CONST_CGS_SPEED_OF_LIGHT);
A68_ENV_REAL (genie_cgs_standard_gas_volume, GSL_CONST_CGS_STANDARD_GAS_VOLUME);
A68_ENV_REAL (genie_cgs_std_atmosphere, GSL_CONST_CGS_STD_ATMOSPHERE) A68_ENV_REAL (genie_cgs_stilb, GSL_CONST_CGS_STILB);
A68_ENV_REAL (genie_cgs_stokes, GSL_CONST_CGS_STOKES) A68_ENV_REAL (genie_cgs_tablespoon, GSL_CONST_CGS_TABLESPOON);
A68_ENV_REAL (genie_cgs_teaspoon, GSL_CONST_CGS_TEASPOON) A68_ENV_REAL (genie_cgs_texpoint, GSL_CONST_CGS_TEXPOINT);
A68_ENV_REAL (genie_cgs_therm, GSL_CONST_CGS_THERM) A68_ENV_REAL (genie_cgs_ton, GSL_CONST_CGS_TON);
A68_ENV_REAL (genie_cgs_torr, GSL_CONST_CGS_TORR) A68_ENV_REAL (genie_cgs_troy_ounce, GSL_CONST_CGS_TROY_OUNCE);
A68_ENV_REAL (genie_cgs_uk_gallon, GSL_CONST_CGS_UK_GALLON) A68_ENV_REAL (genie_cgs_uk_ton, GSL_CONST_CGS_UK_TON);
A68_ENV_REAL (genie_cgs_unified_atomic_mass, GSL_CONST_CGS_UNIFIED_ATOMIC_MASS);
A68_ENV_REAL (genie_cgs_us_gallon, GSL_CONST_CGS_US_GALLON) A68_ENV_REAL (genie_cgs_week, GSL_CONST_CGS_WEEK);
A68_ENV_REAL (genie_cgs_yard, GSL_CONST_CGS_YARD) A68_ENV_REAL (genie_mks_acre, GSL_CONST_MKS_ACRE);
A68_ENV_REAL (genie_mks_angstrom, GSL_CONST_MKS_ANGSTROM);
A68_ENV_REAL (genie_mks_astronomical_unit, GSL_CONST_MKS_ASTRONOMICAL_UNIT) A68_ENV_REAL (genie_mks_bar, GSL_CONST_MKS_BAR);
A68_ENV_REAL (genie_mks_barn, GSL_CONST_MKS_BARN) A68_ENV_REAL (genie_mks_bohr_magneton, GSL_CONST_MKS_BOHR_MAGNETON);
A68_ENV_REAL (genie_mks_bohr_radius, GSL_CONST_MKS_BOHR_RADIUS) A68_ENV_REAL (genie_mks_boltzmann, GSL_CONST_MKS_BOLTZMANN);
A68_ENV_REAL (genie_mks_btu, GSL_CONST_MKS_BTU) A68_ENV_REAL (genie_mks_calorie, GSL_CONST_MKS_CALORIE);
A68_ENV_REAL (genie_mks_canadian_gallon, GSL_CONST_MKS_CANADIAN_GALLON) A68_ENV_REAL (genie_mks_carat, GSL_CONST_MKS_CARAT);
A68_ENV_REAL (genie_mks_cup, GSL_CONST_MKS_CUP) A68_ENV_REAL (genie_mks_curie, GSL_CONST_MKS_CURIE);
A68_ENV_REAL (genie_mks_day, GSL_CONST_MKS_DAY) A68_ENV_REAL (genie_mks_dyne, GSL_CONST_MKS_DYNE);
A68_ENV_REAL (genie_mks_electron_charge, GSL_CONST_MKS_ELECTRON_CHARGE);
A68_ENV_REAL (genie_mks_electron_magnetic_moment, GSL_CONST_MKS_ELECTRON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_mks_electron_volt, GSL_CONST_MKS_ELECTRON_VOLT) A68_ENV_REAL (genie_mks_erg, GSL_CONST_MKS_ERG);
A68_ENV_REAL (genie_mks_faraday, GSL_CONST_MKS_FARADAY) A68_ENV_REAL (genie_mks_fathom, GSL_CONST_MKS_FATHOM);
A68_ENV_REAL (genie_mks_fluid_ounce, GSL_CONST_MKS_FLUID_OUNCE) A68_ENV_REAL (genie_mks_foot, GSL_CONST_MKS_FOOT);
A68_ENV_REAL (genie_mks_footcandle, GSL_CONST_MKS_FOOTCANDLE) A68_ENV_REAL (genie_mks_footlambert, GSL_CONST_MKS_FOOTLAMBERT);
A68_ENV_REAL (genie_mks_gauss, GSL_CONST_MKS_GAUSS) A68_ENV_REAL (genie_mks_gram_force, GSL_CONST_MKS_GRAM_FORCE);
A68_ENV_REAL (genie_mks_grav_accel, GSL_CONST_MKS_GRAV_ACCEL);
A68_ENV_REAL (genie_mks_gravitational_constant, GSL_CONST_MKS_GRAVITATIONAL_CONSTANT);
A68_ENV_REAL (genie_mks_hectare, GSL_CONST_MKS_HECTARE) A68_ENV_REAL (genie_mks_horsepower, GSL_CONST_MKS_HORSEPOWER);
A68_ENV_REAL (genie_mks_hour, GSL_CONST_MKS_HOUR) A68_ENV_REAL (genie_mks_inch, GSL_CONST_MKS_INCH);
A68_ENV_REAL (genie_mks_inch_of_mercury, GSL_CONST_MKS_INCH_OF_MERCURY);
A68_ENV_REAL (genie_mks_inch_of_water, GSL_CONST_MKS_INCH_OF_WATER) A68_ENV_REAL (genie_mks_joule, GSL_CONST_MKS_JOULE);
A68_ENV_REAL (genie_mks_kilometers_per_hour, GSL_CONST_MKS_KILOMETERS_PER_HOUR);
A68_ENV_REAL (genie_mks_kilopound_force, GSL_CONST_MKS_KILOPOUND_FORCE) A68_ENV_REAL (genie_mks_knot, GSL_CONST_MKS_KNOT);
A68_ENV_REAL (genie_mks_lambert, GSL_CONST_MKS_LAMBERT) A68_ENV_REAL (genie_mks_light_year, GSL_CONST_MKS_LIGHT_YEAR);
A68_ENV_REAL (genie_mks_liter, GSL_CONST_MKS_LITER) A68_ENV_REAL (genie_mks_lumen, GSL_CONST_MKS_LUMEN);
A68_ENV_REAL (genie_mks_lux, GSL_CONST_MKS_LUX) A68_ENV_REAL (genie_mks_mass_electron, GSL_CONST_MKS_MASS_ELECTRON);
A68_ENV_REAL (genie_mks_mass_muon, GSL_CONST_MKS_MASS_MUON) A68_ENV_REAL (genie_mks_mass_neutron, GSL_CONST_MKS_MASS_NEUTRON);
A68_ENV_REAL (genie_mks_mass_proton, GSL_CONST_MKS_MASS_PROTON);
A68_ENV_REAL (genie_mks_meter_of_mercury, GSL_CONST_MKS_METER_OF_MERCURY);
A68_ENV_REAL (genie_mks_metric_ton, GSL_CONST_MKS_METRIC_TON) A68_ENV_REAL (genie_mks_micron, GSL_CONST_MKS_MICRON);
A68_ENV_REAL (genie_mks_mil, GSL_CONST_MKS_MIL) A68_ENV_REAL (genie_mks_mile, GSL_CONST_MKS_MILE);
A68_ENV_REAL (genie_mks_miles_per_hour, GSL_CONST_MKS_MILES_PER_HOUR) A68_ENV_REAL (genie_mks_minute, GSL_CONST_MKS_MINUTE);
A68_ENV_REAL (genie_mks_molar_gas, GSL_CONST_MKS_MOLAR_GAS) A68_ENV_REAL (genie_mks_nautical_mile, GSL_CONST_MKS_NAUTICAL_MILE);
A68_ENV_REAL (genie_mks_newton, GSL_CONST_MKS_NEWTON) A68_ENV_REAL (genie_mks_nuclear_magneton, GSL_CONST_MKS_NUCLEAR_MAGNETON);
A68_ENV_REAL (genie_mks_ounce_mass, GSL_CONST_MKS_OUNCE_MASS) A68_ENV_REAL (genie_mks_parsec, GSL_CONST_MKS_PARSEC);
A68_ENV_REAL (genie_mks_phot, GSL_CONST_MKS_PHOT) A68_ENV_REAL (genie_mks_pint, GSL_CONST_MKS_PINT);
A68_ENV_REAL (genie_mks_planck_constant_h, 6.6260693e-34) A68_ENV_REAL (genie_mks_planck_constant_hbar, 6.6260693e-34 / (2 * A68G_PI));
A68_ENV_REAL (genie_mks_point, GSL_CONST_MKS_POINT) A68_ENV_REAL (genie_mks_poise, GSL_CONST_MKS_POISE);
A68_ENV_REAL (genie_mks_pound_force, GSL_CONST_MKS_POUND_FORCE) A68_ENV_REAL (genie_mks_pound_mass, GSL_CONST_MKS_POUND_MASS);
A68_ENV_REAL (genie_mks_poundal, GSL_CONST_MKS_POUNDAL);
A68_ENV_REAL (genie_mks_proton_magnetic_moment, GSL_CONST_MKS_PROTON_MAGNETIC_MOMENT);
A68_ENV_REAL (genie_mks_psi, GSL_CONST_MKS_PSI) A68_ENV_REAL (genie_mks_quart, GSL_CONST_MKS_QUART);
A68_ENV_REAL (genie_mks_rad, GSL_CONST_MKS_RAD) A68_ENV_REAL (genie_mks_roentgen, GSL_CONST_MKS_ROENTGEN);
A68_ENV_REAL (genie_mks_rydberg, GSL_CONST_MKS_RYDBERG) A68_ENV_REAL (genie_mks_solar_mass, GSL_CONST_MKS_SOLAR_MASS);
A68_ENV_REAL (genie_mks_speed_of_light, GSL_CONST_MKS_SPEED_OF_LIGHT);
A68_ENV_REAL (genie_mks_standard_gas_volume, GSL_CONST_MKS_STANDARD_GAS_VOLUME);
A68_ENV_REAL (genie_mks_std_atmosphere, GSL_CONST_MKS_STD_ATMOSPHERE) A68_ENV_REAL (genie_mks_stilb, GSL_CONST_MKS_STILB);
A68_ENV_REAL (genie_mks_stokes, GSL_CONST_MKS_STOKES) A68_ENV_REAL (genie_mks_tablespoon, GSL_CONST_MKS_TABLESPOON);
A68_ENV_REAL (genie_mks_teaspoon, GSL_CONST_MKS_TEASPOON) A68_ENV_REAL (genie_mks_texpoint, GSL_CONST_MKS_TEXPOINT);
A68_ENV_REAL (genie_mks_therm, GSL_CONST_MKS_THERM) A68_ENV_REAL (genie_mks_ton, GSL_CONST_MKS_TON);
A68_ENV_REAL (genie_mks_torr, GSL_CONST_MKS_TORR) A68_ENV_REAL (genie_mks_troy_ounce, GSL_CONST_MKS_TROY_OUNCE);
A68_ENV_REAL (genie_mks_uk_gallon, GSL_CONST_MKS_UK_GALLON) A68_ENV_REAL (genie_mks_uk_ton, GSL_CONST_MKS_UK_TON);
A68_ENV_REAL (genie_mks_unified_atomic_mass, GSL_CONST_MKS_UNIFIED_ATOMIC_MASS);
A68_ENV_REAL (genie_mks_us_gallon, GSL_CONST_MKS_US_GALLON);
A68_ENV_REAL (genie_mks_vacuum_permeability, GSL_CONST_MKS_VACUUM_PERMEABILITY);
A68_ENV_REAL (genie_mks_vacuum_permittivity, GSL_CONST_MKS_VACUUM_PERMITTIVITY) A68_ENV_REAL (genie_mks_week, GSL_CONST_MKS_WEEK);
A68_ENV_REAL (genie_mks_yard, GSL_CONST_MKS_YARD) A68_ENV_REAL (genie_num_atto, GSL_CONST_NUM_ATTO);
A68_ENV_REAL (genie_num_avogadro, GSL_CONST_NUM_AVOGADRO) A68_ENV_REAL (genie_num_exa, GSL_CONST_NUM_EXA);
A68_ENV_REAL (genie_num_femto, GSL_CONST_NUM_FEMTO) A68_ENV_REAL (genie_num_fine_structure, GSL_CONST_NUM_FINE_STRUCTURE);
A68_ENV_REAL (genie_num_giga, GSL_CONST_NUM_GIGA) A68_ENV_REAL (genie_num_kilo, GSL_CONST_NUM_KILO);
A68_ENV_REAL (genie_num_mega, GSL_CONST_NUM_MEGA) A68_ENV_REAL (genie_num_micro, GSL_CONST_NUM_MICRO);
A68_ENV_REAL (genie_num_milli, GSL_CONST_NUM_MILLI) A68_ENV_REAL (genie_num_nano, GSL_CONST_NUM_NANO);
A68_ENV_REAL (genie_num_peta, GSL_CONST_NUM_PETA) A68_ENV_REAL (genie_num_pico, GSL_CONST_NUM_PICO);
A68_ENV_REAL (genie_num_tera, GSL_CONST_NUM_TERA) A68_ENV_REAL (genie_num_yocto, GSL_CONST_NUM_YOCTO);
A68_ENV_REAL (genie_num_yotta, GSL_CONST_NUM_YOTTA) A68_ENV_REAL (genie_num_zepto, GSL_CONST_NUM_ZEPTO);
A68_ENV_REAL (genie_num_zetta, GSL_CONST_NUM_ZETTA);
#endif

/* Macros. */

#define C_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (VALUE (x));\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define OWN_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (p, VALUE (x));\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define GSL_FUNCTION(p, f)\
  A68_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  RESET_ERRNO;\
  VALUE (x) = f (VALUE (x));\
  math_rte (p, errno != 0, MODE (REAL), NULL);

#define GSL_COMPLEX_FUNCTION(f)\
  gsl_complex x, z;\
  A68_REAL *rex, *imx;\
  imx = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));\
  rex = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));\
  GSL_SET_COMPLEX (&x, VALUE (rex), VALUE (imx));\
  (void) gsl_set_error_handler_off ();\
  RESET_ERRNO;\
  z = f (x);\
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);\
  VALUE (imx) = GSL_IMAG(z);\
  VALUE (rex) = GSL_REAL(z)

#define GSL_1_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), &y);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = y.val

#define GSL_2_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

#define GSL_2_INT_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f ((int) VALUE (x), VALUE (y), &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

#define GSL_3_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z),  &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

#define GSL_1D_FUNCTION(p, f)\
  A68_REAL *x;\
  gsl_sf_result y;\
  int status;\
  POP_OPERAND_ADDRESS (p, x, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), GSL_PREC_DOUBLE, &y);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = y.val

#define GSL_2D_FUNCTION(p, f)\
  A68_REAL *x, *y;\
  gsl_sf_result r;\
  int status;\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

#define GSL_3D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

#define GSL_4D_FUNCTION(p, f)\
  A68_REAL *x, *y, *z, *rho;\
  gsl_sf_result r;\
  int status;\
  POP_ADDRESS (p, rho, A68_REAL);\
  POP_ADDRESS (p, z, A68_REAL);\
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);\
  (void) gsl_set_error_handler_off ();\
  status = f (VALUE (x), VALUE (y), VALUE (z), VALUE (rho), GSL_PREC_DOUBLE, &r);\
  math_rte (p, status != 0, MODE (REAL), gsl_strerror (status));\
  VALUE (x) = r.val

/*!
\brief the cube root of x
\param x x
\return same
**/

     double curt (double x)
{
#define CBRT2 1.2599210498948731647672;
#define CBRT4 1.5874010519681994747517;
  int expo, sign;
  double z, x0;
  static double y[11] = {
    7.937005259840997e-01,
    8.193212706006459e-01,
    8.434326653017493e-01,
    8.662391053409029e-01,
    8.879040017426008e-01,
    9.085602964160699e-01,
    9.283177667225558e-01,
    9.472682371859097e-01,
    9.654893846056298e-01,
    9.830475724915586e-01,
    1.0
  };
  if (x == 0.0 || x == 1.0) {
    return (x);
  }
  if (x > 0.0) {
    sign = 1;
  } else {
    sign = -1;
    x = -x;
  }
  x = frexp (x, &expo);
/* Cube root in [0.5, 1] by Newton's method. */
  z = x;
  x = y[(int) (20 * x - 10)];
  x0 = 0;
  while (ABS (x - x0) > DBL_EPSILON) {
    x0 = x;
    x = (z / (x * x) + x + x) / 3;
  }
/* Get exponent. */
  if (expo >= 0) {
    int j = expo % 3;
    if (j == 1) {
      x *= CBRT2;
    } else if (j == 2) {
      x *= CBRT4;
    }
    expo /= 3;
  } else {
    int j = (-expo) % 3;
    if (j == 1) {
      x /= CBRT2;
    } else if (j == 2) {
      x /= CBRT4;
    }
    expo = -(-expo) / 3;
  }
  x = ldexp (x, expo);
  return (sign >= 0 ? x : -x);
}

/*!
\brief inverse complementary error function
\param y y
\return same
**/

double inverfc (double y)
{
  if (y < 0.0 || y > 2.0) {
    errno = EDOM;
    return (0.0);
  } else if (y == 0.0) {
    return (DBL_MAX);
  } else if (y == 1.0) {
    return (0.0);
  } else if (y == 2.0) {
    return (-DBL_MAX);
  } else {
/* Next is adapted code from a package that contains following statement:
   Copyright (c) 1996 Takuya Ooura.
   You may use, copy, modify this code for any purpose and without fee. */
    double s, t, u, v, x, z;
    if (y <= 1.0) {
      z = y;
    } else {
      z = 2.0 - y;
    }
    v = 0.916461398268964 - log (z);
    u = sqrt (v);
    s = (log (u) + 0.488826640273108) / v;
    t = 1.0 / (u + 0.231729200323405);
    x = u * (1.0 - s * (s * 0.124610454613712 + 0.5)) - ((((-0.0728846765585675 * t + 0.269999308670029) * t + 0.150689047360223) * t + 0.116065025341614) * t + 0.499999303439796) * t;
    t = 3.97886080735226 / (x + 3.97886080735226);
    u = t - 0.5;
    s = (((((((((0.00112648096188977922 * u + 1.05739299623423047e-4) * u - 0.00351287146129100025) * u - 7.71708358954120939e-4) * u + 0.00685649426074558612) * u + 0.00339721910367775861) * u - 0.011274916933250487) * u - 0.0118598117047771104) * u + 0.0142961988697898018) * u + 0.0346494207789099922) * u + 0.00220995927012179067;
    s = ((((((((((((s * u - 0.0743424357241784861) * u - 0.105872177941595488) * u + 0.0147297938331485121) * u + 0.316847638520135944) * u + 0.713657635868730364) * u + 1.05375024970847138) * u + 1.21448730779995237) * u + 1.16374581931560831) * u + 0.956464974744799006) * u + 0.686265948274097816) * u + 0.434397492331430115) * u + 0.244044510593190935) * t - z * exp (x * x - 0.120782237635245222);
    x += s * (x * s + 1.0);
    return (y <= 1.0 ? x : -x);
  }
}

/*!
\brief inverse error function
\param y y
\return same
**/

double inverf (double y)
{
  return (inverfc (1 - y));
}

/*!
\brief PROC sqrt = (REAL) REAL
\param p position in tree
**/

void genie_sqrt_real (NODE_T * p)
{
  C_FUNCTION (p, sqrt);
}

/*!
\brief PROC curt = (REAL) REAL
\param p position in tree
**/

void genie_curt_real (NODE_T * p)
{
  C_FUNCTION (p, curt);
}

/*!
\brief PROC exp = (REAL) REAL
\param p position in tree
**/

void genie_exp_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_exp);
}

/*!
\brief PROC ln = (REAL) REAL
\param p position in tree
**/

void genie_ln_real (NODE_T * p)
{
  C_FUNCTION (p, log);
}

/*!
\brief PROC log = (REAL) REAL
\param p position in tree
**/

void genie_log_real (NODE_T * p)
{
  C_FUNCTION (p, log10);
}

/*!
\brief PROC sin = (REAL) REAL
\param p position in tree
**/

void genie_sin_real (NODE_T * p)
{
  C_FUNCTION (p, sin);
}

/*!
\brief PROC arcsin = (REAL) REAL
\param p position in tree
**/

void genie_arcsin_real (NODE_T * p)
{
  C_FUNCTION (p, asin);
}

/*!
\brief PROC cos = (REAL) REAL
\param p position in tree
**/

void genie_cos_real (NODE_T * p)
{
  C_FUNCTION (p, cos);
}

/*!
\brief PROC arccos = (REAL) REAL
\param p position in tree
**/

void genie_arccos_real (NODE_T * p)
{
  C_FUNCTION (p, acos);
}

/*!
\brief PROC tan = (REAL) REAL
\param p position in tree
**/

void genie_tan_real (NODE_T * p)
{
  C_FUNCTION (p, tan);
}

/*!
\brief PROC arctan = (REAL) REAL
\param p position in tree
**/

void genie_arctan_real (NODE_T * p)
{
  C_FUNCTION (p, atan);
}

/*!
\brief PROC arctan2 = (REAL) REAL
\param p position in tree
**/

void genie_atan2_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  RESET_ERRNO;
  if (VALUE (x) == 0.0 && VALUE (y) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (LONG_REAL), NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  VALUE (x) = a68g_atan2 (VALUE (y), VALUE (x));
  if (errno != 0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_MATH_EXCEPTION);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief PROC sinh = (REAL) REAL
\param p position in tree
**/

void genie_sinh_real (NODE_T * p)
{
  C_FUNCTION (p, sinh);
}

/*!
\brief PROC cosh = (REAL) REAL
\param p position in tree
**/

void genie_cosh_real (NODE_T * p)
{
  C_FUNCTION (p, cosh);
}

/*!
\brief PROC tanh = (REAL) REAL
\param p position in tree
**/

void genie_tanh_real (NODE_T * p)
{
  C_FUNCTION (p, tanh);
}

/*!
\brief PROC arcsinh = (REAL) REAL
\param p position in tree
**/

void genie_arcsinh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_asinh);
}

/*!
\brief PROC arccosh = (REAL) REAL
\param p position in tree
**/

void genie_arccosh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_acosh);
}

/*!
\brief PROC arctanh = (REAL) REAL
\param p position in tree
**/

void genie_arctanh_real (NODE_T * p)
{
  C_FUNCTION (p, a68g_atanh);
}

/*!
\brief PROC inverse erf = (REAL) REAL
\param p position in tree
**/

void genie_inverf_real (NODE_T * p)
{
  C_FUNCTION (p, inverf);
}

/*!
\brief PROC inverse erfc = (REAL) REAL 
\param p position in tree
**/

void genie_inverfc_real (NODE_T * p)
{
  C_FUNCTION (p, inverfc);
}

/*!
\brief PROC lj e 12 6 = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_lj_e_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  double u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  RESET_ERRNO;
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 4.0 * VALUE (e) * u6 * (u6 - 1.0);
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC lj f 12 6 = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_lj_f_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  double u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  RESET_ERRNO;
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 24.0 * VALUE (e) * u2 * u6 * (1.0 - 2.0 * u6);
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

#if defined ENABLE_NUMERICAL

/* "Special" functions - but what is so "special" about them? */

/*!
\brief PROC erf = (REAL) REAL
\param p position in tree
**/

void genie_erf_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erf_e);
}

/*!
\brief PROC erfc = (REAL) REAL
\param p position in tree
**/

void genie_erfc_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_erfc_e);
}

/*!
\brief PROC gamma = (REAL) REAL
\param p position in tree
**/

void genie_gamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_gamma_e);
}

/*!
\brief PROC gamma incomplete = (REAL, REAL) REAL
\param p position in tree
**/

void genie_gamma_inc_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_gamma_inc_P_e);
}

/*!
\brief PROC lngamma = (REAL) REAL
\param p position in tree
**/

void genie_lngamma_real (NODE_T * p)
{
  GSL_1_FUNCTION (p, gsl_sf_lngamma_e);
}

/*!
\brief PROC factorial = (REAL) REAL
\param p position in tree
**/

void genie_factorial_real (NODE_T * p)
{
/* gsl_sf_fact reduces argument to int, hence we do gamma (x + 1) */
  A68_REAL *x = (A68_REAL *) STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL));
  VALUE (x) += 1.0;
  {
    GSL_1_FUNCTION (p, gsl_sf_gamma_e);
  }
}

/*!
\brief PROC beta = (REAL, REAL) REAL
\param p position in tree
**/

void genie_beta_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_beta_e);
}

/*!
\brief PROC beta incomplete = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_beta_inc_real (NODE_T * p)
{
  GSL_3_FUNCTION (p, gsl_sf_beta_inc_e);
}

/*!
\brief PROC airy ai = (REAL) REAL
\param p position in tree
**/

void genie_airy_ai_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_e);
}

/*!
\brief PROC airy bi = (REAL) REAL
\param p position in tree
**/

void genie_airy_bi_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_e);
}

/*!
\brief PROC airy ai derivative = (REAL) REAL
\param p position in tree
**/

void genie_airy_ai_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Ai_deriv_e);
}

/*!
\brief PROC airy bi derivative = (REAL) REAL
\param p position in tree
**/

void genie_airy_bi_deriv_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_airy_Bi_deriv_e);
}

/*!
\brief PROC bessel jn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Jn_e);
}

/*!
\brief PROC bessel yn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_yn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Yn_e);
}

/*!
\brief PROC bessel in = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_e);
}

/*!
\brief PROC bessel exp in = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_in_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_In_scaled_e);
}

/*!
\brief PROC bessel kn = (REAL, REAL) REAL 
\param p position in tree
**/

void genie_bessel_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_e);
}

/*!
\brief PROC bessel exp kn = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_kn_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_Kn_scaled_e);
}

/*!
\brief PROC bessel jl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_jl_e);
}

/*!
\brief PROC bessel yl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_yl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_yl_e);
}

/*!
\brief PROC bessel exp il = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_il_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_il_scaled_e);
}

/*!
\brief PROC bessel exp kl = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_kl_real (NODE_T * p)
{
  GSL_2_INT_FUNCTION (p, gsl_sf_bessel_kl_scaled_e);
}

/*!
\brief PROC bessel jnu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_jnu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Jnu_e);
}

/*!
\brief PROC bessel ynu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_ynu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Ynu_e);
}

/*!
\brief PROC bessel inu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_e);
}

/*!
\brief PROC bessel exp inu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_inu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Inu_scaled_e);
}

/*!
\brief PROC bessel knu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_e);
}

/*!
\brief PROC bessel exp knu = (REAL, REAL) REAL
\param p position in tree
**/

void genie_bessel_exp_knu_real (NODE_T * p)
{
  GSL_2_FUNCTION (p, gsl_sf_bessel_Knu_scaled_e);
}

/*!
\brief PROC elliptic integral k = (REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_k_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Kcomp_e);
}

/*!
\brief PROC elliptic integral e = (REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_e_real (NODE_T * p)
{
  GSL_1D_FUNCTION (p, gsl_sf_ellint_Ecomp_e);
}

/*!
\brief PROC elliptic integral rf = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rf_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RF_e);
}

/*!
\brief PROC elliptic integral rd = (REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rd_real (NODE_T * p)
{
  GSL_3D_FUNCTION (p, gsl_sf_ellint_RD_e);
}

/*!
\brief PROC elliptic integral rj = (REAL, REAL, REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rj_real (NODE_T * p)
{
  GSL_4D_FUNCTION (p, gsl_sf_ellint_RJ_e);
}

/*!
\brief PROC elliptic integral rc = (REAL, REAL) REAL
\param p position in tree
**/

void genie_elliptic_integral_rc_real (NODE_T * p)
{
  GSL_2D_FUNCTION (p, gsl_sf_ellint_RC_e);
}

#endif

/*
Next part is a "stand-alone" version of GNU Scientific Library (GSL)
random number generator "taus113", based on GSL file "rng/taus113.c" that
has the notice:

Copyright (C) 2002 Atakan Gurkan
Based on the file taus.c which has the notice
Copyright (C) 1996, 1997, 1998, 1999, 2000 James Theiler, Brian Gough.

This is a maximally equidistributed combined, collision free
Tausworthe generator, with a period ~2^113 (~10^34).
The sequence is

x_n = (z1_n ^ z2_n ^ z3_n ^ z4_n)

b = (((z1_n <<  6) ^ z1_n) >> 13)
z1_{n+1} = (((z1_n & 4294967294) << 18) ^ b)
b = (((z2_n <<  2) ^ z2_n) >> 27)
z2_{n+1} = (((z2_n & 4294967288) <<  2) ^ b)
b = (((z3_n << 13) ^ z3_n) >> 21)
z3_{n+1} = (((z3_n & 4294967280) <<  7) ^ b)
b = (((z4_n <<  3)  ^ z4_n) >> 12)
z4_{n+1} = (((z4_n & 4294967168) << 13) ^ b)

computed modulo 2^32. In the formulas above '^' means exclusive-or
(C-notation), not exponentiation.
The algorithm is for 32-bit integers, hence a bitmask is used to clear
all but least significant 32 bits, after left shifts, to make the code
work on architectures where integers are 64-bit.

The generator is initialized with
zi = (69069 * z{i+1}) MOD 2^32 where z0 is the seed provided
During initialization a check is done to make sure that the initial seeds
have a required number of their most significant bits set.
After this, the state is passed through the RNG 10 times to ensure the
state satisfies a recurrence relation.

References:
P. L'Ecuyer, "Tables of Maximally-Equidistributed Combined LFSR Generators",
Mathematics of Computation, 68, 225 (1999), 261--269.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps
P. L'Ecuyer, "Maximally Equidistributed Combined Tausworthe Generators",
Mathematics of Computation, 65, 213 (1996), 203--213.
  http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme.ps
the online version of the latter contains corrections to the print version.
*/

#define LCG(n) ((69069UL * n) & 0xffffffffUL)
#define TAUSWORTHE_MASK 0xffffffffUL

typedef struct
{
  unsigned long int z1, z2, z3, z4;
} taus113_state_t;

static taus113_state_t rng_state;

static unsigned long int taus113_get (taus113_state_t * state);
static void taus113_set (taus113_state_t * state, unsigned long int s);

/*!
\brief taus113_get
\param state state
\return same
**/

static unsigned long taus113_get (taus113_state_t * state)
{
  unsigned long b1, b2, b3, b4;
  b1 = ((((state->z1 << 6UL) & TAUSWORTHE_MASK) ^ state->z1) >> 13UL);
  state->z1 = ((((state->z1 & 4294967294UL) << 18UL) & TAUSWORTHE_MASK) ^ b1);
  b2 = ((((state->z2 << 2UL) & TAUSWORTHE_MASK) ^ state->z2) >> 27UL);
  state->z2 = ((((state->z2 & 4294967288UL) << 2UL) & TAUSWORTHE_MASK) ^ b2);
  b3 = ((((state->z3 << 13UL) & TAUSWORTHE_MASK) ^ state->z3) >> 21UL);
  state->z3 = ((((state->z3 & 4294967280UL) << 7UL) & TAUSWORTHE_MASK) ^ b3);
  b4 = ((((state->z4 << 3UL) & TAUSWORTHE_MASK) ^ state->z4) >> 12UL);
  state->z4 = ((((state->z4 & 4294967168UL) << 13UL) & TAUSWORTHE_MASK) ^ b4);
  return (state->z1 ^ state->z2 ^ state->z3 ^ state->z4);
}

/*!
\brief taus113_set
\param state state
\param s s
**/

static void taus113_set (taus113_state_t * state, unsigned long int s)
{
  if (!s) {
/* default seed is 1 */
    s = 1UL;
  }
  state->z1 = LCG (s);
  if (state->z1 < 2UL) {
    state->z1 += 2UL;
  }
  state->z2 = LCG (state->z1);
  if (state->z2 < 8UL) {
    state->z2 += 8UL;
  }
  state->z3 = LCG (state->z2);
  if (state->z3 < 16UL) {
    state->z3 += 16UL;
  }
  state->z4 = LCG (state->z3);
  if (state->z4 < 128UL) {
    state->z4 += 128UL;
  }
/* Calling RNG ten times to satify recurrence condition. */
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
}

/*!
\brief initialise rng
\param u initialiser
**/

void init_rng (unsigned long u)
{
  taus113_set (&rng_state, u);
}

/*!
\brief rng 53 bit
\return same
**/

double rng_53_bit (void)
{
  double a = (double) (taus113_get (&rng_state) >> 5);
  double b = (double) (taus113_get (&rng_state) >> 6);
  return (a * /* 2^26 */ 67108864.0 + b) / /* 2^53 */ 9007199254740992.0;
}

/*
Rules for analytic calculations using GNU Emacs Calc:
(used to find the values for the test program)

[ LCG(n) := n * 69069 mod (2^32) ]

[ b1(x) := rsh(xor(lsh(x, 6), x), 13),
q1(x) := xor(lsh(and(x, 4294967294), 18), b1(x)),
b2(x) := rsh(xor(lsh(x, 2), x), 27),
q2(x) := xor(lsh(and(x, 4294967288), 2), b2(x)),
b3(x) := rsh(xor(lsh(x, 13), x), 21),
q3(x) := xor(lsh(and(x, 4294967280), 7), b3(x)),
b4(x) := rsh(xor(lsh(x, 3), x), 12),
q4(x) := xor(lsh(and(x, 4294967168), 13), b4(x))
]

[ S([z1,z2,z3,z4]) := [q1(z1), q2(z2), q3(z3), q4(z4)] ]		 
*/

/*
This file also contains Algol68G's standard environ for complex numbers.
Some of the LONG operations are generic for LONG and LONG LONG.

Some routines are based on
* GNU Scientific Library
* Abramowitz and Stegun.
*/

#if defined ENABLE_NUMERICAL

#include <gsl/gsl_complex.h>
#include <gsl/gsl_complex_math.h>
#include <gsl/gsl_const.h>
#include <gsl/gsl_errno.h>
#include <gsl/gsl_math.h>
#include <gsl/gsl_sf.h>

#define GSL_COMPLEX_FUNCTION(f)\
  gsl_complex x, z;\
  A68_REAL *rex, *imx;\
  imx = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));\
  rex = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));\
  GSL_SET_COMPLEX (&x, VALUE (rex), VALUE (imx));\
  (void) gsl_set_error_handler_off ();\
  RESET_ERRNO;\
  z = f (x);\
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);\
  VALUE (imx) = GSL_IMAG(z);\
  VALUE (rex) = GSL_REAL(z)

#endif

/*!
\brief OP +* = (REAL, REAL) COMPLEX
\param p position in tree
**/

void genie_icomplex (NODE_T * p)
{
  (void) p;
}

/*!
\brief OP +* = (INT, INT) COMPLEX
\param p position in tree
**/

void genie_iint_complex (NODE_T * p)
{
  A68_INT re, im;
  POP_OBJECT (p, &im, A68_INT);
  POP_OBJECT (p, &re, A68_INT);
  PUSH_PRIMITIVE (p, (double) VALUE (&re), A68_REAL);
  PUSH_PRIMITIVE (p, (double) VALUE (&im), A68_REAL);
}

/*!
\brief OP RE = (COMPLEX) REAL
\param p position in tree
**/

void genie_re_complex (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, ALIGNED_SIZE_OF (A68_REAL));
}

/*!
\brief OP IM = (COMPLEX) REAL
\param p position in tree
**/

void genie_im_complex (NODE_T * p)
{
  A68_REAL im;
  POP_OBJECT (p, &im, A68_REAL);
  *(A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL))) = im;
}

/*!
\brief OP - = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_minus_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x;
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) = -VALUE (im_x);
  VALUE (re_x) = -VALUE (re_x);
  (void) p;
}

/*!
\brief ABS = (COMPLEX) REAL
\param p position in tree
**/

void genie_abs_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, a68g_hypot (VALUE (&re_x), VALUE (&im_x)), A68_REAL);
}

/*!
\brief OP ARG = (COMPLEX) REAL
\param p position in tree
**/

void genie_arg_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  if (VALUE (&re_x) != 0.0 || VALUE (&im_x) != 0.0) {
    PUSH_PRIMITIVE (p, atan2 (VALUE (&im_x), VALUE (&re_x)), A68_REAL);
  } else {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (COMPLEX), NULL);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

/*!
\brief OP CONJ = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_conj_complex (NODE_T * p)
{
  A68_REAL *im;
  POP_OPERAND_ADDRESS (p, im, A68_REAL);
  VALUE (im) = -VALUE (im);
}

/*!
\brief OP + = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_add_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) += VALUE (&im_y);
  VALUE (re_x) += VALUE (&re_y);
  TEST_COMPLEX_REPRESENTATION (p, VALUE (re_x), VALUE (im_x));
}

/*!
\brief OP - = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sub_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  VALUE (im_x) -= VALUE (&im_y);
  VALUE (re_x) -= VALUE (&re_y);
  TEST_COMPLEX_REPRESENTATION (p, VALUE (re_x), VALUE (im_x));
}

/*!
\brief OP * = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_mul_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re, im;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  re = VALUE (&re_x) * VALUE (&re_y) - VALUE (&im_x) * VALUE (&im_y);
  im = VALUE (&im_x) * VALUE (&re_y) + VALUE (&re_x) * VALUE (&im_y);
  TEST_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP / = (COMPLEX, COMPLEX) COMPLEX
\param p position in tree
**/

void genie_div_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re = 0.0, im = 0.0;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
#if ! defined ENABLE_IEEE_754
  if (VALUE (&re_y) == 0.0 && VALUE (&im_y) == 0.0) {
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (COMPLEX));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
#endif
  if (ABS (VALUE (&re_y)) >= ABS (VALUE (&im_y))) {
    double r = VALUE (&im_y) / VALUE (&re_y), den = VALUE (&re_y) + r * VALUE (&im_y);
    re = (VALUE (&re_x) + r * VALUE (&im_x)) / den;
    im = (VALUE (&im_x) - r * VALUE (&re_x)) / den;
  } else {
    double r = VALUE (&re_y) / VALUE (&im_y), den = VALUE (&im_y) + r * VALUE (&re_y);
    re = (VALUE (&re_x) * r + VALUE (&im_x)) / den;
    im = (VALUE (&im_x) * r - VALUE (&re_x)) / den;
  }
  TEST_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP ** = (COMPLEX, INT) COMPLEX
\param p position in tree
**/

void genie_pow_complex_int (NODE_T * p)
{
  A68_REAL re_x, im_x;
  double re_y, im_y, re_z, im_z, rea;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  POP_COMPLEX (p, &re_x, &im_x);
  re_z = 1.0;
  im_z = 0.0;
  re_y = VALUE (&re_x);
  im_y = VALUE (&im_x);
  expo = 1;
  negative = (VALUE (&j) < 0.0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (expo & VALUE (&j)) {
      rea = re_z * re_y - im_z * im_y;
      im_z = re_z * im_y + im_z * re_y;
      re_z = rea;
    }
    rea = re_y * re_y - im_y * im_y;
    im_y = im_y * re_y + re_y * im_y;
    re_y = rea;
    expo <<= 1;
  }
  TEST_COMPLEX_REPRESENTATION (p, re_z, im_z);
  if (negative) {
    PUSH_PRIMITIVE (p, 1.0, A68_REAL);
    PUSH_PRIMITIVE (p, 0.0, A68_REAL);
    PUSH_PRIMITIVE (p, re_z, A68_REAL);
    PUSH_PRIMITIVE (p, im_z, A68_REAL);
    genie_div_complex (p);
  } else {
    PUSH_PRIMITIVE (p, re_z, A68_REAL);
    PUSH_PRIMITIVE (p, im_z, A68_REAL);
  }
}

/*!
\brief OP = = (COMPLEX, COMPLEX) BOOL
\param p position in tree
**/

void genie_eq_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, (VALUE (&re_x) == VALUE (&re_y)) && (VALUE (&im_x) == VALUE (&im_y)), A68_BOOL);
}

/*!
\brief OP /= = (COMPLEX, COMPLEX) BOOL
\param p position in tree
**/

void genie_ne_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_PRIMITIVE (p, !((VALUE (&re_x) == VALUE (&re_y))
                       && (VALUE (&im_x) == VALUE (&im_y))), A68_BOOL);
}

/*!
\brief OP +:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_plusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_add_complex);
}

/*!
\brief OP -:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_minusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_sub_complex);
}

/*!
\brief OP *:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_timesab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_mul_complex);
}

/*!
\brief OP /:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in tree
**/

void genie_divab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_div_complex);
}

/*!
\brief OP LENG = (COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_lengthen_complex_to_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  A68_REAL a, b;
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  STACK_MP (z, p, digits);
  real_to_mp (p, z, VALUE (&a), digits);
  MP_STATUS (z) = INITIALISED_MASK;
  STACK_MP (z, p, digits);
  real_to_mp (p, z, VALUE (&b), digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG COMPLEX) COMPLEX
\param p position in tree
**/

void genie_shorten_long_complex_to_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, mp_to_real (p, a, digits), A68_REAL);
  PUSH_PRIMITIVE (p, mp_to_real (p, b, digits), A68_REAL);
}

/*!
\brief OP LENG = (LONG COMPLEX) LONG LONG COMPLEX
\param p position in tree
**/

void genie_lengthen_long_complex_to_longlong_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  int digs_long = get_mp_digits (MODE (LONGLONG_REAL)), size_long = get_mp_size (MODE (LONGLONG_REAL));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *a, *b, *c, *d;
  b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  STACK_MP (c, p, digs_long);
  STACK_MP (d, p, digs_long);
  lengthen_mp (p, c, digs_long, a, digits);
  lengthen_mp (p, d, digs_long, b, digits);
  MOVE_MP (a, c, digs_long);
  MOVE_MP (&a[2 + digs_long], d, digs_long);
  stack_pointer = pop_sp;
  MP_STATUS (a) = INITIALISED_MASK;
  (&a[2 + digs_long])[0] = INITIALISED_MASK;
  INCREMENT_STACK_POINTER (p, 2 * (size_long - size));
}

/*!
\brief OP SHORTEN = (LONG LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_shorten_longlong_complex_to_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  int digs_long = get_mp_digits (MODE (LONGLONG_REAL)), size_long = get_mp_size (MODE (LONGLONG_REAL));
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *a, *b;
  b = (MP_DIGIT_T *) STACK_OFFSET (-size_long);
  a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size_long);
  shorten_mp (p, a, digits, a, digs_long);
  shorten_mp (p, &a[2 + digits], digits, b, digs_long);
  stack_pointer = pop_sp;
  MP_STATUS (a) = INITIALISED_MASK;
  (&a[2 + digits])[0] = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, 2 * (size_long - size));
}

/*!
\brief OP RE = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_re_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_STATUS (a) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
}

/*!
\brief OP IM = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_im_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (LHS_MODE (p)), size = get_mp_size (MOID (PACK (MOID (p))));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MOVE_MP (a, b, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
}

/*!
\brief OP - = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_minus_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (a, 1) = -MP_DIGIT (a, 1);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
}

/*!
\brief OP CONJ = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_conj_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
}

/*!
\brief OP ABS = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_abs_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  hypot_mp (p, z, a, b, digits);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  MOVE_MP (a, z, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief OP ARG = (LONG COMPLEX) LONG REAL
\param p position in tree
**/

void genie_arg_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *z;
  STACK_MP (z, p, digits);
  atan2_mp (p, z, a, b, digits);
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  MOVE_MP (a, z, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief OP + = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_add_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *d = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *c = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  add_mp (p, b, b, d, digits);
  add_mp (p, a, a, c, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP - = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sub_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *d = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *c = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  sub_mp (p, b, b, d, digits);
  sub_mp (p, a, a, c, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP * = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_mul_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *d = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *c = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  cmul_mp (p, a, b, c, d, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP / = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_div_long_complex (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *d = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *c = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  cdiv_mp (p, a, b, c, d, digits);
  MP_STATUS (a) = INITIALISED_MASK;
  MP_STATUS (b) = INITIALISED_MASK;
  stack_pointer = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

/*!
\brief OP ** = (LONG COMPLEX, INT) LONG COMPLEX
\param p position in tree
**/

void genie_pow_long_complex_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp;
  MP_DIGIT_T *re_x, *im_x, *re_y, *im_y, *re_z, *im_z, *rea, *acc;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  pop_sp = stack_pointer;
  im_x = (MP_DIGIT_T *) STACK_OFFSET (-size);
  re_x = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  STACK_MP (re_z, p, digits);
  set_mp_short (re_z, (MP_DIGIT_T) 1, 0, digits);
  STACK_MP (im_z, p, digits);
  SET_MP_ZERO (im_z, digits);
  STACK_MP (re_y, p, digits);
  STACK_MP (im_y, p, digits);
  MOVE_MP (re_y, re_x, digits);
  MOVE_MP (im_y, im_x, digits);
  STACK_MP (rea, p, digits);
  STACK_MP (acc, p, digits);
  expo = 1;
  negative = (VALUE (&j) < 0.0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((unsigned) expo <= (unsigned) (VALUE (&j))) {
    if (expo & VALUE (&j)) {
      mul_mp (p, acc, im_z, im_y, digits);
      mul_mp (p, rea, re_z, re_y, digits);
      sub_mp (p, rea, rea, acc, digits);
      mul_mp (p, acc, im_z, re_y, digits);
      mul_mp (p, im_z, re_z, im_y, digits);
      add_mp (p, im_z, im_z, acc, digits);
      MOVE_MP (re_z, rea, digits);
    }
    mul_mp (p, acc, im_y, im_y, digits);
    mul_mp (p, rea, re_y, re_y, digits);
    sub_mp (p, rea, rea, acc, digits);
    mul_mp (p, acc, im_y, re_y, digits);
    mul_mp (p, im_y, re_y, im_y, digits);
    add_mp (p, im_y, im_y, acc, digits);
    MOVE_MP (re_y, rea, digits);
    expo <<= 1;
  }
  stack_pointer = pop_sp;
  if (negative) {
    set_mp_short (re_x, (MP_DIGIT_T) 1, 0, digits);
    SET_MP_ZERO (im_x, digits);
    INCREMENT_STACK_POINTER (p, 2 * size);
    genie_div_long_complex (p);
  } else {
    MOVE_MP (re_x, re_z, digits);
    MOVE_MP (im_x, im_z, digits);
  }
  MP_STATUS (re_x) = INITIALISED_MASK;
  MP_STATUS (im_x) = INITIALISED_MASK;
}

/*!
\brief OP = = (LONG COMPLEX, LONG COMPLEX) BOOL
\param p position in tree
**/

void genie_eq_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, MP_DIGIT (a, 1) == 0 && MP_DIGIT (b, 1) == 0, A68_BOOL);
}

/*!
\brief OP /= = (LONG COMPLEX, LONG COMPLEX) BOOL
\param p position in tree
**/

void genie_ne_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_PRIMITIVE (p, MP_DIGIT (a, 1) != 0 || MP_DIGIT (b, 1) != 0, A68_BOOL);
}

/*!
\brief OP +:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_plusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_complex);
}

/*!
\brief OP -:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_minusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_complex);
}

/*!
\brief OP *:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_timesab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_complex);
}

/*!
\brief OP /:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in tree
**/

void genie_divab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_complex);
}

/*!
\brief PROC csqrt = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sqrt_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (re) == 0.0 && VALUE (im) == 0.0) {
    VALUE (re) = 0.0;
    VALUE (im) = 0.0;
  } else {
    double x = ABS (VALUE (re)), y = ABS (VALUE (im)), w;
    if (x >= y) {
      double t = y / x;
      w = sqrt (x) * sqrt (0.5 * (1.0 + sqrt (1.0 + t * t)));
    } else {
      double t = x / y;
      w = sqrt (y) * sqrt (0.5 * (t + sqrt (1.0 + t * t)));
    }
    if (VALUE (re) >= 0.0) {
      VALUE (re) = w;
      VALUE (im) = VALUE (im) / (2.0 * w);
    } else {
      double ai = VALUE (im);
      double vi = (ai >= 0.0 ? w : -w);
      VALUE (re) = ai / (2.0 * vi);
      VALUE (im) = vi;
    }
  }
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long csqrt = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sqrt_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  RESET_ERRNO;
  csqrt_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC cexp = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_exp_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  double r;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  r = exp (VALUE (re));
  VALUE (re) = r * cos (VALUE (im));
  VALUE (im) = r * sin (VALUE (im));
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long cexp = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_exp_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  cexp_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC cln = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_ln_complex (NODE_T * p)
{
  A68_REAL *re, *im, r, th;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  PUSH_COMPLEX (p, VALUE (re), VALUE (im));
  genie_abs_complex (p);
  POP_OBJECT (p, &r, A68_REAL);
  PUSH_COMPLEX (p, VALUE (re), VALUE (im));
  genie_arg_complex (p);
  POP_OBJECT (p, &th, A68_REAL);
  VALUE (re) = log (VALUE (&r));
  VALUE (im) = VALUE (&th);
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long cln = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_ln_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  cln_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC csin = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sin_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (im) == 0.0) {
    VALUE (re) = sin (VALUE (re));
    VALUE (im) = 0.0;
  } else {
    double r = VALUE (re), i = VALUE (im);
    VALUE (re) = sin (r) * cosh (i);
    VALUE (im) = cos (r) * sinh (i);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long csin = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_sin_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  csin_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC ccos = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_cos_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (VALUE (im) == 0.0) {
    VALUE (re) = cos (VALUE (re));
    VALUE (im) = 0.0;
  } else {
    double r = VALUE (re), i = VALUE (im);
    VALUE (re) = cos (r) * cosh (i);
    VALUE (im) = sin (r) * sinh (-i);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long ccos = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_cos_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  ccos_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC ctan = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_tan_complex (NODE_T * p)
{
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  double r, i;
  A68_REAL u, v;
  RESET_ERRNO;
  r = VALUE (re);
  i = VALUE (im);
  PUSH_PRIMITIVE (p, r, A68_REAL);
  PUSH_PRIMITIVE (p, i, A68_REAL);
  genie_sin_complex (p);
  POP_OBJECT (p, &v, A68_REAL);
  POP_OBJECT (p, &u, A68_REAL);
  PUSH_PRIMITIVE (p, r, A68_REAL);
  PUSH_PRIMITIVE (p, i, A68_REAL);
  genie_cos_complex (p);
  VALUE (re) = VALUE (&u);
  VALUE (im) = VALUE (&v);
  genie_div_complex (p);
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long ctan = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_tan_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  ctan_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC carcsin= (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arcsin_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = asin (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    VALUE (re) = asin (b);
    VALUE (im) = log (a + sqrt (a * a - 1));
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long arcsin = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_asin_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  casin_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC carccos = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arccos_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = acos (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    VALUE (re) = acos (b);
    VALUE (im) = -log (a + sqrt (a * a - 1));
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long carccos = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_acos_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  cacos_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

/*!
\brief PROC carctan = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arctan_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * ALIGNED_SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-ALIGNED_SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    VALUE (re) = atan (VALUE (re));
  } else {
    double r = VALUE (re), i = VALUE (im);
    double a = a68g_hypot (r, i + 1), b = a68g_hypot (r, i - 1);
    VALUE (re) = 0.5 * atan (2 * r / (1 - r * r - i * i));
    VALUE (im) = 0.5 * log (a / b);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long catan = (LONG COMPLEX) LONG COMPLEX
\param p position in tree
**/

void genie_atan_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digits = get_mp_digits (mode), size = get_mp_size (mode);
  ADDR_T pop_sp = stack_pointer;
  MP_DIGIT_T *re = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT_T *im = (MP_DIGIT_T *) STACK_OFFSET (-size);
  RESET_ERRNO;
  catan_mp (p, re, im, digits);
  stack_pointer = pop_sp;
  MP_STATUS (re) = INITIALISED_MASK;
  MP_STATUS (im) = INITIALISED_MASK;
  math_rte (p, errno != 0, mode, NULL);
}

#if defined ENABLE_NUMERICAL

/*!
\brief PROC csinh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_sinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_sinh);
}

/*!
\brief PROC ccosh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_cosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_cosh);
}

/*!
\brief PROC ctanh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_tanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_tanh);
}

/*!
\brief PROC carcsinh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arcsinh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arcsinh);
}

/*!
\brief PROC carccosh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arccosh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arccosh);
}

/*!
\brief PROC carctanh = (COMPLEX) COMPLEX
\param p position in tree
**/

void genie_arctanh_complex (NODE_T * p)
{
  GSL_COMPLEX_FUNCTION (gsl_complex_arctanh);
}

#endif /* ENABLE_NUMERICAL */
