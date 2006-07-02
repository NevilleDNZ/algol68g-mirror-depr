/*!
\file complex.c
\brief standard environ routines for complex numbers
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
This file contains Algol68G's standard environ for complex numbers.
Some of the LONG operations are generic for LONG and LONG LONG.

Some routines are based on
* GNU Scientific Library
* Abramowitz and Stegun.
*/

#include "algol68g.h"
#include "genie.h"
#include "mp.h"


/*!
\brief OP +* = (REAL, REAL) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_icomplex (NODE_T * p)
{
  (void) p;
}

/*!
\brief OP +* = (INT, INT) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_iint_complex (NODE_T * p)
{
  A68_INT re, im;
  POP_INT (p, &im);
  POP_INT (p, &re);
  PUSH_REAL (p, (double) re.value);
  PUSH_REAL (p, (double) im.value);
}

/*!
\brief OP RE = (COMPLEX) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_re_complex (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, SIZE_OF (A68_REAL));
}

/*!
\brief OP IM = (COMPLEX) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_im_complex (NODE_T * p)
{
  A68_REAL im;
  POP_REAL (p, &im);
  *(A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL))) = im;
}

/*!
\brief OP - = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_minus_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x;
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  im_x->value = -im_x->value;
  re_x->value = -re_x->value;
  (void) p;
}

/*!
\brief ABS = (COMPLEX) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_abs_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_REAL (p, a68g_hypot (re_x.value, im_x.value));
}

/*!
\brief OP ARG = (COMPLEX) REAL
\param p position in syntax tree, should not be NULL
**/

void genie_arg_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  if (re_x.value != 0.0 || im_x.value != 0.0) {
    PUSH_REAL (p, atan2 (im_x.value, re_x.value));
  } else {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_INVALID_ARGUMENT, MODE (COMPLEX), NULL);
    exit_genie (p, A_RUNTIME_ERROR);
  }
}

/*!
\brief OP CONJ = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_conj_complex (NODE_T * p)
{
  A68_REAL *im;
  POP_OPERAND_ADDRESS (p, im, A68_REAL);
  im->value = -im->value;
}

/*!
\brief OP + = (COMPLEX, COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_add_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  im_x->value += im_y.value;
  re_x->value += re_y.value;
  TEST_COMPLEX_REPRESENTATION (p, re_x->value, im_x->value);
}

/*!
\brief OP - = (COMPLEX, COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_sub_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  im_x->value -= im_y.value;
  re_x->value -= re_y.value;
  TEST_COMPLEX_REPRESENTATION (p, re_x->value, im_x->value);
}

/*!
\brief OP * = (COMPLEX, COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_mul_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re, im;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  re = re_x.value * re_y.value - im_x.value * im_y.value;
  im = im_x.value * re_y.value + re_x.value * im_y.value;
  TEST_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP / = (COMPLEX, COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_div_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  double re = 0.0, im = 0.0;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
#ifndef HAVE_IEEE_754
  if (re_y.value == 0.0 && im_y.value == 0.0) {
    diagnostic_node (A_RUNTIME_ERROR, p, ERROR_DIVISION_BY_ZERO, MODE (COMPLEX));
    exit_genie (p, A_RUNTIME_ERROR);
  }
#endif
  if (ABS (re_y.value) >= ABS (im_y.value)) {
    double r = im_y.value / re_y.value, den = re_y.value + r * im_y.value;
    re = (re_x.value + r * im_x.value) / den;
    im = (im_x.value - r * re_x.value) / den;
  } else {
    double r = re_y.value / im_y.value, den = im_y.value + r * re_y.value;
    re = (re_x.value * r + im_x.value) / den;
    im = (im_x.value * r - re_x.value) / den;
  }
  TEST_COMPLEX_REPRESENTATION (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

/*!
\brief OP ** = (COMPLEX, INT) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_pow_complex_int (NODE_T * p)
{
  A68_REAL re_x, im_x;
  double re_y, im_y, re_z, im_z, rea;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_INT (p, &j);
  POP_COMPLEX (p, &re_x, &im_x);
  re_z = 1.0;
  im_z = 0.0;
  re_y = re_x.value;
  im_y = im_x.value;
  expo = 1;
  negative = (j.value < 0.0);
  if (negative) {
    j.value = -j.value;
  }
  while ((unsigned) expo <= (unsigned) (j.value)) {
    if (expo & j.value) {
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
    PUSH_REAL (p, 1.0);
    PUSH_REAL (p, 0.0);
    PUSH_REAL (p, re_z);
    PUSH_REAL (p, im_z);
    genie_div_complex (p);
  } else {
    PUSH_REAL (p, re_z);
    PUSH_REAL (p, im_z);
  }
}

/*!
\brief OP = = (COMPLEX, COMPLEX) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_eq_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_BOOL (p, (re_x.value == re_y.value) && (im_x.value == im_y.value));
}

/*!
\brief OP /= = (COMPLEX, COMPLEX) BOOL
\param p position in syntax tree, should not be NULL
\return
**/

void genie_ne_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_BOOL (p, !((re_x.value == re_y.value) && (im_x.value == im_y.value)));
}

/*!
\brief OP +:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_add_complex);
}

/*!
\brief OP -:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_sub_complex);
}

/*!
\brief OP *:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_mul_complex);
}

/*!
\brief OP /:= = (REF COMPLEX, COMPLEX) REF COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_divab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, MODE (REF_COMPLEX), genie_div_complex);
}

/*!
\brief OP LENG = (COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_lengthen_complex_to_long_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL));
  MP_DIGIT_T *z;
  A68_REAL a, b;
  POP_REAL (p, &b);
  POP_REAL (p, &a);
  STACK_MP (z, p, digits);
  real_to_mp (p, z, a.value, digits);
  MP_STATUS (z) = INITIALISED_MASK;
  STACK_MP (z, p, digits);
  real_to_mp (p, z, b.value, digits);
  MP_STATUS (z) = INITIALISED_MASK;
}

/*!
\brief OP SHORTEN = (LONG COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_shorten_long_complex_to_complex (NODE_T * p)
{
  int digits = get_mp_digits (MODE (LONG_REAL)), size = get_mp_size (MODE (LONG_REAL));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-2 * size);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_REAL (p, mp_to_real (p, a, digits));
  PUSH_REAL (p, mp_to_real (p, b, digits));
}

/*!
\brief OP LENG = (LONG COMPLEX) LONG LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
\return
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
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
  POP_INT (p, &j);
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
  negative = (j.value < 0.0);
  if (negative) {
    j.value = -j.value;
  }
  while ((unsigned) expo <= (unsigned) (j.value)) {
    if (expo & j.value) {
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
\param p position in syntax tree, should not be NULL
**/

void genie_eq_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_BOOL (p, MP_DIGIT (a, 1) == 0 && MP_DIGIT (b, 1) == 0);
}

/*!
\brief OP /= = (LONG COMPLEX, LONG COMPLEX) BOOL
\param p position in syntax tree, should not be NULL
**/

void genie_ne_long_complex (NODE_T * p)
{
  int size = get_mp_size (LHS_MODE (p));
  MP_DIGIT_T *b = (MP_DIGIT_T *) STACK_OFFSET (-3 * size);
  MP_DIGIT_T *a = (MP_DIGIT_T *) STACK_OFFSET (-4 * size);
  genie_sub_long_complex (p);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_BOOL (p, MP_DIGIT (a, 1) != 0 || MP_DIGIT (b, 1) != 0);
}

/*!
\brief OP +:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_plusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_long_complex);
}

/*!
\brief OP -:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_minusab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_long_complex);
}

/*!
\brief OP *:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_timesab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_long_complex);
}

/*!
\brief OP /:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_divab_long_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_long_complex);
}

/*!
\brief PROC csqrt = (COMPLEX) COMPLEX
\param p position in syntax tree, should not be NULL
**/

void genie_sqrt_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (re->value == 0.0 && im->value == 0.0) {
    re->value = 0.0;
    im->value = 0.0;
  } else {
    double x = ABS (re->value), y = ABS (im->value), w;
    if (x >= y) {
      double t = y / x;
      w = sqrt (x) * sqrt (0.5 * (1.0 + sqrt (1.0 + t * t)));
    } else {
      double t = x / y;
      w = sqrt (y) * sqrt (0.5 * (t + sqrt (1.0 + t * t)));
    }
    if (re->value >= 0.0) {
      re->value = w;
      im->value = im->value / (2.0 * w);
    } else {
      double ai = im->value;
      double vi = (ai >= 0.0 ? w : -w);
      re->value = ai / (2.0 * vi);
      im->value = vi;
    }
  }
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long csqrt = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_exp_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  double r;
  im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  r = exp (re->value);
  re->value = r * cos (im->value);
  im->value = r * sin (im->value);
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long cexp = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_ln_complex (NODE_T * p)
{
  A68_REAL *re, *im, r, th;
  im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  PUSH_COMPLEX (p, re->value, im->value);
  genie_abs_complex (p);
  POP_REAL (p, &r);
  PUSH_COMPLEX (p, re->value, im->value);
  genie_arg_complex (p);
  POP_REAL (p, &th);
  re->value = log (r.value);
  im->value = th.value;
  math_rte (p, errno != 0, MODE (COMPLEX), NULL);
}

/*!
\brief PROC long cln = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_sin_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im->value == 0.0) {
    re->value = sin (re->value);
    im->value = 0.0;
  } else {
    double r = re->value, i = im->value;
    re->value = sin (r) * cosh (i);
    im->value = cos (r) * sinh (i);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long csin = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_cos_complex (NODE_T * p)
{
  A68_REAL *re, *im;
  im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im->value == 0.0) {
    re->value = cos (re->value);
    im->value = 0.0;
  } else {
    double r = re->value, i = im->value;
    re->value = cos (r) * cosh (i);
    im->value = sin (r) * sinh (-i);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long ccos = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_tan_complex (NODE_T * p)
{
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  double r, i;
  A68_REAL u, v;
  RESET_ERRNO;
  r = re->value;
  i = im->value;
  PUSH_REAL (p, r);
  PUSH_REAL (p, i);
  genie_sin_complex (p);
  POP_REAL (p, &v);
  POP_REAL (p, &u);
  PUSH_REAL (p, r);
  PUSH_REAL (p, i);
  genie_cos_complex (p);
  re->value = u.value;
  im->value = v.value;
  genie_div_complex (p);
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long ctan = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
\return
**/

void genie_arcsin_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    re->value = asin (re->value);
  } else {
    double r = re->value, i = im->value;
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    re->value = asin (b);
    im->value = log (a + sqrt (a * a - 1));
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long arcsin = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_arccos_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    re->value = acos (re->value);
  } else {
    double r = re->value, i = im->value;
    double u = a68g_hypot (r + 1, i), v = a68g_hypot (r - 1, i);
    double a = 0.5 * (u + v), b = 0.5 * (u - v);
    re->value = acos (b);
    im->value = -log (a + sqrt (a * a - 1));
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long carccos = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
\param p position in syntax tree, should not be NULL
**/

void genie_arctan_complex (NODE_T * p)
{
  A68_REAL *re = (A68_REAL *) (STACK_OFFSET (-2 * SIZE_OF (A68_REAL)));
  A68_REAL *im = (A68_REAL *) (STACK_OFFSET (-SIZE_OF (A68_REAL)));
  RESET_ERRNO;
  if (im == 0) {
    re->value = atan (re->value);
  } else {
    double r = re->value, i = im->value;
    double a = a68g_hypot (r, i + 1), b = a68g_hypot (r, i - 1);
    re->value = 0.5 * atan (2 * r / (1 - r * r - i * i));
    im->value = 0.5 * log (a / b);
  }
  math_rte (p, errno != 0, MODE (REAL), NULL);
}

/*!
\brief PROC long catan = (LONG COMPLEX) LONG COMPLEX
\param p position in syntax tree, should not be NULL
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
