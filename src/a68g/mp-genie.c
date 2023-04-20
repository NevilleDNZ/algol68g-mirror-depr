//! @file mp-misc.c
//! @author J. Marcel van der Veer
//!
//! @section Copyright
//!
//! This file is part of Algol68G - an Algol 68 compiler-interpreter.
//! Copyright 2001-2023 J. Marcel van der Veer [algol68g@xs4all.nl].
//!
//! @section License
//!
//! This program is free software; you can redistribute it and/or modify it 
//! under the terms of the GNU General Public License as published by the 
//! Free Software Foundation; either version 3 of the License, or 
//! (at your option) any later version.
//!
//! This program is distributed in the hope that it will be useful, but 
//! WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY 
//! or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License for 
//! more details. You should have received a copy of the GNU General Public 
//! License along with this program. If not, see [http://www.gnu.org/licenses/].

//! @section Synopsis
//!
//! Multi-precision interpreter routines.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-mp.h"

//! brief LONG REAL long infinity

void genie_infinity_mp (NODE_T *p)
{
  int digs = DIGITS (MOID (p));
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (PLUS_INF_MASK | INIT_MASK);
}

//! brief LONG REAL long minus infinity

void genie_minus_infinity_mp (NODE_T *p)
{
  int digs = DIGITS (MOID (p));
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (MINUS_INF_MASK | INIT_MASK);
}

//! @brief LONG INT long max int

void genie_long_max_int (NODE_T * p)
{
  int digs = DIGITS (M_LONG_INT);
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (digs - 1);
  for (unt k = 1; k <= digs; k++) {
    MP_DIGIT (z, k) = (MP_T) (MP_RADIX - 1);
  }
}

//! @brief LONG LONG INT long long max int

void genie_long_mp_max_int (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_INT);
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (digs - 1);
  for (unt k = 1; k <= digs; k++) {
    MP_DIGIT (z, k) = (MP_T) (MP_RADIX - 1);
  }
}

//! @brief LONG REAL long max real

void genie_long_max_real (NODE_T * p)
{
  unt digs = DIGITS (M_LONG_REAL);
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (MAX_MP_EXPONENT - 1);
  for (unt k = 1; k <= digs; k++) {
    MP_DIGIT (z, k) = (MP_T) (MP_RADIX - 1);
  }
}

//! @brief LONG LONG REAL long long max real

void genie_long_mp_max_real (NODE_T * p)
{
  unt digs = DIGITS (M_LONG_LONG_REAL);
  MP_T *z = nil_mp (p, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_EXPONENT (z) = (MP_T) (MAX_MP_EXPONENT - 1);
  for (unt k = 1; k <= digs; k++) {
    MP_DIGIT (z, k) = (MP_T) (MP_RADIX - 1);
  }
}

//! @brief LONG REAL min long real

void genie_long_min_real (NODE_T * p)
{
  (void) lit_mp (p, 1, -MAX_MP_EXPONENT, DIGITS (M_LONG_REAL));
}

//! @brief LONG LONG REAL min long long real

void genie_long_mp_min_real (NODE_T * p)
{
  (void) lit_mp (p, 1, -MAX_MP_EXPONENT, DIGITS (M_LONG_LONG_REAL));
}

//! @brief LONG REAL small long real

void genie_long_small_real (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL);
  (void) lit_mp (p, 1, 1 - digs, digs);
}

//! @brief LONG LONG REAL small long long real

void genie_long_mp_small_real (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_REAL);
  (void) lit_mp (p, 1, 1 - digs, digs);
}

//! @brief OP LENG = (INT) LONG INT

void genie_lengthen_int_to_mp (NODE_T * p)
{
  int digs = DIGITS (M_LONG_INT);
  A68_INT k;
  POP_OBJECT (p, &k, A68_INT);
  MP_T *z = nil_mp (p, digs);
  (void) int_to_mp (p, z, VALUE (&k), digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG INT) INT

void genie_shorten_mp_to_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  PUSH_VALUE (p, mp_to_int (p, z, digs), A68_INT);
}

//! @brief OP LENG = (REAL) LONG REAL

void genie_lengthen_real_to_mp (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL);
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  MP_T *z = nil_mp (p, digs);
  (void) real_to_mp (p, z, VALUE (&x), digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG REAL) REAL

void genie_shorten_mp_to_real (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  PUSH_VALUE (p, mp_to_real (p, z, digs), A68_REAL);
}

//! @brief OP ENTIER = (LONG REAL) LONG INT

void genie_entier_mp (NODE_T * p)
{
  int digs = DIGITS (LHS_MODE (p)), size = SIZE (LHS_MODE (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  (void) entier_mp (p, z, z, digs);
  A68_SP = pop_sp;
}

#define C_L_FUNCTION(p, f)\
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));\
  ADDR_T pop_sp = A68_SP;\
  MP_T *x = (MP_T *) STACK_OFFSET (-size);\
  errno = 0;\
  PRELUDE_ERROR (f (p, x, x, digs) == NaN_MP || errno != 0, p, ERROR_INVALID_ARGUMENT, MOID (p));\
  MP_STATUS (x) = (MP_T) INIT_MASK;\
  A68_SP = pop_sp;

//! @brief PROC (LONG REAL) LONG REAL long sqrt

void genie_sqrt_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sqrt_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long curt

void genie_curt_mp (NODE_T * p)
{
  C_L_FUNCTION (p, curt_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long exp

void genie_exp_mp (NODE_T * p)
{
  C_L_FUNCTION (p, exp_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long erf

void genie_erf_mp (NODE_T * p)
{
  C_L_FUNCTION (p, erf_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long inverf

void genie_inverf_mp (NODE_T * p)
{
  C_L_FUNCTION (p, inverf_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long erfc

void genie_erfc_mp (NODE_T * p)
{
  C_L_FUNCTION (p, erfc_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long inverfc

void genie_inverfc_mp (NODE_T * p)
{
  C_L_FUNCTION (p, inverfc_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long gamma

void genie_gamma_mp (NODE_T * p)
{
  C_L_FUNCTION (p, gamma_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long ln gamma

void genie_lngamma_mp (NODE_T * p)
{
  C_L_FUNCTION (p, lngamma_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long beta

void genie_beta_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  PRELUDE_ERROR (beta_mp (p, a, a, b, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  A68_SP -= size;
  MP_STATUS (a) = (MP_T) INIT_MASK;
}

//! @brief PROC (LONG REAL) LONG REAL long ln beta

void genie_lnbeta_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  PRELUDE_ERROR (lnbeta_mp (p, a, a, b, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  A68_SP -= size;
  MP_STATUS (a) = (MP_T) INIT_MASK;
}

//! @brief PROC (LONG REAL) LONG REAL long beta

void genie_beta_inc_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *t = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *s = (MP_T *) STACK_OFFSET (-3 * size);
  PRELUDE_ERROR (beta_inc_mp (p, s, s, t, x, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  A68_SP -= 2 * size;
  MP_STATUS (s) = (MP_T) INIT_MASK;
}

//! @brief PROC (LONG REAL) LONG REAL long ln

void genie_ln_mp (NODE_T * p)
{
  C_L_FUNCTION (p, ln_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long log

void genie_log_mp (NODE_T * p)
{
  C_L_FUNCTION (p, log_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long sinh

void genie_sinh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sinh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long cosh

void genie_cosh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cosh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long tanh

void genie_tanh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, tanh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arcsinh

void genie_asinh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, asinh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arccosh

void genie_acosh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acosh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arctanh

void genie_atanh_mp (NODE_T * p)
{
  C_L_FUNCTION (p, atanh_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long sin

void genie_sin_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sin_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long cos

void genie_cos_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cos_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long tan

void genie_tan_mp (NODE_T * p)
{
  C_L_FUNCTION (p, tan_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arcsin

void genie_asin_mp (NODE_T * p)
{
  C_L_FUNCTION (p, asin_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arccos

void genie_acos_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acos_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long arctan

void genie_atan_mp (NODE_T * p)
{
  C_L_FUNCTION (p, atan_mp);
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL long arctan2

void genie_atan2_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  PRELUDE_ERROR (atan2_mp (p, x, y, x, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  A68_SP -= size;
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

// Arithmetic operations.

//! @brief OP LENG = (LONG MODE) LONG LONG MODE

void genie_lengthen_mp_to_long_mp (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, (int) size_mp ());
  MP_T *z = (MP_T *) STACK_ADDRESS (A68_SP);
  z = len_mp (p, z, mp_digits (), long_mp_digits ());
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG LONG MODE) LONG MODE

void genie_shorten_long_mp_to_mp (NODE_T * p)
{
  MOID_T *m = SUB_MOID (p);
  DECREMENT_STACK_POINTER (p, (int) size_long_mp ());
  MP_T *z = empty_mp (p, mp_digits ());
  if (m == M_LONG_INT) {
    PRELUDE_ERROR (MP_EXPONENT (z) > LONG_MP_DIGITS - 1, p, ERROR_OUT_OF_BOUNDS, m);
  }
  (void) shorten_mp (p, z, mp_digits (), z, long_mp_digits ());
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP - = (LONG MODE) LONG MODE

void genie_minus_mp (NODE_T * p)
{
  int size = SIZE (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
}

//! @brief OP ABS = (LONG MODE) LONG MODE

void genie_abs_mp (NODE_T * p)
{
  int size = SIZE (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
}

//! @brief OP SIGN = (LONG MODE) INT

void genie_sign_mp (NODE_T * p)
{
  int size = SIZE (LHS_MODE (p));
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  PUSH_VALUE (p, SIGN (MP_DIGIT (z, 1)), A68_INT);
}

//! @brief OP + = (LONG MODE, LONG MODE) LONG MODE

void genie_add_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) add_mp (p, x, x, y, digs);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP - = (LONG MODE, LONG MODE) LONG MODE

void genie_sub_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) sub_mp (p, x, x, y, digs);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP * = (LONG MODE, LONG MODE) LONG MODE

void genie_mul_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) mul_mp (p, x, x, y, digs);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP / = (LONG MODE, LONG MODE) LONG MODE

void genie_div_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (div_mp (p, x, x, y, digs) == NaN_MP, p, ERROR_DIVISION_BY_ZERO, mode);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP % = (LONG MODE, LONG MODE) LONG MODE

void genie_over_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (over_mp (p, x, x, y, digs) == NaN_MP, p, ERROR_DIVISION_BY_ZERO, mode);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP %* = (LONG MODE, LONG MODE) LONG MODE

void genie_mod_mp (NODE_T * p)
{
  MOID_T *mode = RHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  PRELUDE_ERROR (mod_mp (p, x, x, y, digs) == NaN_MP, p, ERROR_DIVISION_BY_ZERO, mode);
  if (MP_DIGIT (x, 1) < 0) {
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    (void) add_mp (p, x, x, y, digs);
  }
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP +:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_plusab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_mp);
}

//! @brief OP -:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_minusab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_mp);
}

//! @brief OP *:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_timesab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_mp);
}

//! @brief OP /:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_divab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_mp);
}

//! @brief OP %:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_overab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_over_mp);
}

//! @brief OP %*:= = (REF LONG MODE, LONG MODE) REF LONG MODE

void genie_modab_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mod_mp);
}

// OP (LONG MODE, LONG MODE) BOOL.

#define A68_CMP_LONG(n, OP)\
void n (NODE_T * p) {\
  MOID_T *mode = LHS_MODE (p);\
  A68_BOOL z;\
  int digs = DIGITS (mode), size = SIZE (mode);\
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);\
  MP_T *y = (MP_T *) STACK_OFFSET (-size);\
  OP (p, &z, x, y, digs);\
  DECREMENT_STACK_POINTER (p, 2 * size);\
  PUSH_VALUE (p, VALUE (&z), A68_BOOL);\
}

A68_CMP_LONG (genie_eq_mp, eq_mp);
A68_CMP_LONG (genie_ne_mp, ne_mp);
A68_CMP_LONG (genie_lt_mp, lt_mp);
A68_CMP_LONG (genie_gt_mp, gt_mp);
A68_CMP_LONG (genie_le_mp, le_mp);
A68_CMP_LONG (genie_ge_mp, ge_mp);

//! @brief OP ** = (LONG MODE, INT) LONG MODE

void genie_pow_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  A68_INT k;
  MP_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_T *) STACK_OFFSET (-size);
  (void) pow_mp_int (p, x, x, VALUE (&k), digs);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

//! @brief OP ** = (LONG MODE, LONG MODE) LONG MODE

void genie_pow_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  if (IS_ZERO_MP (x)) {
    if (MP_DIGIT (y, 1) < (MP_T) 0) {
      PRELUDE_ERROR (A68_TRUE, p, ERROR_INVALID_ARGUMENT, MOID (p));
    } else if (IS_ZERO_MP (y)) {
      SET_MP_ONE (x, digs);
    }
  } else {
    (void) pow_mp (p, x, x, y, digs);
  }
  A68_SP = pop_sp - size;
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

//! @brief OP ODD = (LONG INT) BOOL

void genie_odd_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  DECREMENT_STACK_POINTER (p, size);
  if (MP_EXPONENT (z) <= (MP_T) (digs - 1)) {
    PUSH_VALUE (p, (BOOL_T) ! EVEN ((MP_INT_T) (z[(int) (2 + MP_EXPONENT (z))])), A68_BOOL);
  } else {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  }
}

//! @brief Test whether z is a valid LONG INT.

void test_mp_int_range (NODE_T * p, MP_T * z, MOID_T * m)
{
  PRELUDE_ERROR (!check_mp_int (z, m), p, ERROR_OUT_OF_BOUNDS, m);
}

//! @brief OP + = (LONG INT, LONG INT) LONG INT

void genie_add_mp_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digs = DIGITS (m), size = SIZE (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) add_mp (p, x, x, y, digs);
  test_mp_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP - = (LONG INT, LONG INT) LONG INT

void genie_sub_mp_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digs = DIGITS (m), size = SIZE (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) sub_mp (p, x, x, y, digs);
  test_mp_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP * = (LONG INT, LONG INT) LONG INT

void genie_mul_mp_int (NODE_T * p)
{
  MOID_T *m = RHS_MODE (p);
  int digs = DIGITS (m), size = SIZE (m);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  (void) mul_mp (p, x, x, y, digs);
  test_mp_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP ** = (LONG MODE, INT) LONG INT

void genie_pow_mp_int_int (NODE_T * p)
{
  MOID_T *m = LHS_MODE (p);
  int digs = DIGITS (m), size = SIZE (m);
  A68_INT k;
  MP_T *x;
  POP_OBJECT (p, &k, A68_INT);
  x = (MP_T *) STACK_OFFSET (-size);
  (void) pow_mp_int (p, x, x, VALUE (&k), digs);
  test_mp_int_range (p, x, m);
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

//! @brief OP +:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_plusab_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_mp_int);
}

//! @brief OP -:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_minusab_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_mp_int);
}

//! @brief OP *:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_timesab_mp_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_mp_int);
}

//! @brief OP ROUND = (LONG REAL) LONG INT

void genie_round_mp (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  (void) round_mp (p, z, z, digs);
  A68_SP = pop_sp;
}

#define C_CL_FUNCTION(p, f)\
  MOID_T *mode = MOID (p);\
  int digs = DIGITSC (mode), size = SIZEC (mode);\
  ADDR_T pop_sp = A68_SP;\
  MP_T *im = (MP_T *) STACK_OFFSET (-size);\
  MP_T *re = (MP_T *) STACK_OFFSET (-2 * size);\
  errno = 0;\
  (void) f(p, re, im, digs);\
  A68_SP = pop_sp;\
  MP_STATUS (re) = (MP_T) INIT_MASK;\
  MP_STATUS (im) = (MP_T) INIT_MASK;\
  MATH_RTE (p, errno != 0, mode, NO_TEXT);\

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long csqrt

void genie_sqrt_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, csqrt_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long cexp

void genie_exp_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, cexp_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long cln

void genie_ln_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, cln_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long csin

void genie_sin_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, csin_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long ccos

void genie_cos_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, ccos_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long ctan

void genie_tan_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, ctan_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long arcsin

void genie_asin_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, casin_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long carccos

void genie_acos_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, cacos_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long catan

void genie_atan_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, catan_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long csinh

void genie_sinh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, csinh_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long ccosh

void genie_cosh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, ccosh_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long ctanh

void genie_tanh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, ctanh_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long carcsinh

void genie_asinh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, casinh_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long carccosh

void genie_acosh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, cacosh_mp);
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX long carctanh

void genie_atanh_mp_complex (NODE_T * p)
{
  C_CL_FUNCTION (p, catanh_mp);
}

//! @brief OP LENG = (COMPLEX) LONG COMPLEX

void genie_lengthen_complex_to_mp_complex (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL);
  A68_REAL a, b;
  POP_OBJECT (p, &b, A68_REAL);
  POP_OBJECT (p, &a, A68_REAL);
  MP_T *z = nil_mp (p, digs);
  (void) real_to_mp (p, z, VALUE (&a), digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  z = nil_mp (p, digs);
  (void) real_to_mp (p, z, VALUE (&b), digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG COMPLEX) COMPLEX

void genie_shorten_mp_complex_to_complex (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL), size = SIZE (M_LONG_REAL);
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  DECREMENT_STACK_POINTER (p, 2 * size);
  PUSH_VALUE (p, mp_to_real (p, a, digs), A68_REAL);
  PUSH_VALUE (p, mp_to_real (p, b, digs), A68_REAL);
}

//! @brief OP LENG = (LONG COMPLEX) LONG LONG COMPLEX

void genie_lengthen_mp_complex_to_long_mp_complex (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL), size = SIZE (M_LONG_REAL);
  int gdigs = DIGITS (M_LONG_LONG_REAL), size_g = SIZE (M_LONG_LONG_REAL);
  ADDR_T pop_sp = A68_SP;
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = len_mp (p, a, digs, gdigs);
  MP_T *d = len_mp (p, b, digs, gdigs);
  (void) move_mp (a, c, gdigs);
  (void) move_mp (&a[LEN_MP (gdigs)], d, gdigs);
  A68_SP = pop_sp;
  INCREMENT_STACK_POINTER (p, 2 * (size_g - size));
}

//! @brief OP SHORTEN = (LONG LONG COMPLEX) LONG COMPLEX

void genie_shorten_long_mp_complex_to_mp_complex (NODE_T * p)
{
  int digs = DIGITS (M_LONG_REAL), size = SIZE (M_LONG_REAL);
  int gdigs = DIGITS (M_LONG_LONG_REAL), size_g = SIZE (M_LONG_LONG_REAL);
  ADDR_T pop_sp = A68_SP;
  MP_T *a, *b;
  b = (MP_T *) STACK_OFFSET (-size_g);
  a = (MP_T *) STACK_OFFSET (-2 * size_g);
  (void) shorten_mp (p, a, digs, a, gdigs);
  (void) shorten_mp (p, &a[LEN_MP (digs)], digs, b, gdigs);
  A68_SP = pop_sp;
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (&a[LEN_MP (digs)]) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, 2 * (size_g - size));
}

//! @brief OP RE = (LONG COMPLEX) LONG REAL

void genie_re_mp_complex (NODE_T * p)
{
  int size = SIZE (SUB_MOID (p));
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP IM = (LONG COMPLEX) LONG REAL

void genie_im_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  (void) move_mp (a, b, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  DECREMENT_STACK_POINTER (p, size);
}

//! @brief OP - = (LONG COMPLEX) LONG COMPLEX

void genie_minus_mp_complex (NODE_T * p)
{
  int size = SIZEC (SUB_MOID (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (a, 1) = -MP_DIGIT (a, 1);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
}

//! @brief OP CONJ = (LONG COMPLEX) LONG COMPLEX

void genie_conj_mp_complex (NODE_T * p)
{
  int size = SIZEC (SUB_MOID (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_DIGIT (b, 1) = -MP_DIGIT (b, 1);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
}

//! @brief OP ABS = (LONG COMPLEX) LONG REAL

void genie_abs_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *z = nil_mp (p, digs);
  errno = 0;
  (void) hypot_mp (p, z, a, b, digs);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  (void) move_mp (a, z, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

//! @brief OP ARG = (LONG COMPLEX) LONG REAL

void genie_arg_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *z = nil_mp (p, digs);
  errno = 0;
  (void) atan2_mp (p, z, a, b, digs);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, size);
  (void) move_mp (a, z, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MATH_RTE (p, errno != 0, mode, NO_TEXT);
}

//! @brief OP + = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX

void genie_add_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITSC (mode), size = SIZEC (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) add_mp (p, b, b, d, digs);
  (void) add_mp (p, a, a, c, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

//! @brief OP - = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX

void genie_sub_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITSC (mode), size = SIZEC (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) sub_mp (p, b, b, d, digs);
  (void) sub_mp (p, a, a, c, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

//! @brief OP * = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX

void genie_mul_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITSC (mode), size = SIZEC (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) cmul_mp (p, a, b, c, d, digs);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

//! @brief OP / = (LONG COMPLEX, LONG COMPLEX) LONG COMPLEX

void genie_div_mp_complex (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITSC (mode), size = SIZEC (mode);
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  PRELUDE_ERROR (cdiv_mp (p, a, b, c, d, digs) == NaN_MP, p, ERROR_DIVISION_BY_ZERO, mode);
  MP_STATUS (a) = (MP_T) INIT_MASK;
  MP_STATUS (b) = (MP_T) INIT_MASK;
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 2 * size);
}

//! @brief OP ** = (LONG COMPLEX, INT) LONG COMPLEX

void genie_pow_mp_complex_int (NODE_T * p)
{
  MOID_T *mode = SUB_MOID (p);
  int digs = DIGITSC (mode), size = SIZEC (mode);
  ADDR_T pop_sp;
  A68_INT j;
  int expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  pop_sp = A68_SP;
  MP_T *im_x = (MP_T *) STACK_OFFSET (-size);
  MP_T *re_x = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *re_z = lit_mp (p, 1, 0, digs);
  MP_T *im_z = nil_mp (p, digs);
  MP_T *re_y = nil_mp (p, digs);
  MP_T *im_y = nil_mp (p, digs);
  (void) move_mp (re_y, re_x, digs);
  (void) move_mp (im_y, im_x, digs);
  MP_T *rea = nil_mp (p, digs);
  MP_T *acc = nil_mp (p, digs);
  expo = 1;
  negative = (BOOL_T) (VALUE (&j) < 0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((int) expo <= (int) (VALUE (&j))) {
    if (expo & VALUE (&j)) {
      (void) mul_mp (p, acc, im_z, im_y, digs);
      (void) mul_mp (p, rea, re_z, re_y, digs);
      (void) sub_mp (p, rea, rea, acc, digs);
      (void) mul_mp (p, acc, im_z, re_y, digs);
      (void) mul_mp (p, im_z, re_z, im_y, digs);
      (void) add_mp (p, im_z, im_z, acc, digs);
      (void) move_mp (re_z, rea, digs);
    }
    (void) mul_mp (p, acc, im_y, im_y, digs);
    (void) mul_mp (p, rea, re_y, re_y, digs);
    (void) sub_mp (p, rea, rea, acc, digs);
    (void) mul_mp (p, acc, im_y, re_y, digs);
    (void) mul_mp (p, im_y, re_y, im_y, digs);
    (void) add_mp (p, im_y, im_y, acc, digs);
    (void) move_mp (re_y, rea, digs);
    expo <<= 1;
  }
  A68_SP = pop_sp;
  if (negative) {
    SET_MP_ONE (re_x, digs);
    SET_MP_ZERO (im_x, digs);
    INCREMENT_STACK_POINTER (p, 2 * size);
    genie_div_mp_complex (p);
  } else {
    (void) move_mp (re_x, re_z, digs);
    (void) move_mp (im_x, im_z, digs);
  }
  MP_STATUS (re_x) = (MP_T) INIT_MASK;
  MP_STATUS (im_x) = (MP_T) INIT_MASK;
}

//! @brief OP = = (LONG COMPLEX, LONG COMPLEX) BOOL

void genie_eq_mp_complex (NODE_T * p)
{
  int digs = DIGITSC (LHS_MODE (p)), size = SIZEC (LHS_MODE (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) sub_mp (p, b, b, d, digs);
  (void) sub_mp (p, a, a, c, digs);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 4 * size);
  PUSH_VALUE (p, (BOOL_T) (MP_DIGIT (a, 1) == 0 && MP_DIGIT (b, 1) == 0), A68_BOOL);
}

//! @brief OP /= = (LONG COMPLEX, LONG COMPLEX) BOOL

void genie_ne_mp_complex (NODE_T * p)
{
  int digs = DIGITSC (LHS_MODE (p)), size = SIZEC (LHS_MODE (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *d = (MP_T *) STACK_OFFSET (-size);
  MP_T *c = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *b = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *a = (MP_T *) STACK_OFFSET (-4 * size);
  (void) sub_mp (p, b, b, d, digs);
  (void) sub_mp (p, a, a, c, digs);
  A68_SP = pop_sp;
  DECREMENT_STACK_POINTER (p, 4 * size);
  PUSH_VALUE (p, (BOOL_T) (MP_DIGIT (a, 1) != 0 || MP_DIGIT (b, 1) != 0), A68_BOOL);
}

//! @brief OP +:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX

void genie_plusab_mp_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_add_mp_complex);
}

//! @brief OP -:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX

void genie_minusab_mp_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_sub_mp_complex);
}

//! @brief OP *:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX

void genie_timesab_mp_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_mul_mp_complex);
}

//! @brief OP /:= = (REF LONG COMPLEX, LONG COMPLEX) REF LONG COMPLEX

void genie_divab_mp_complex (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  genie_f_and_becomes (p, mode, genie_div_mp_complex);
}

//! @brief PROC LONG REAL next long random

void genie_long_next_random (NODE_T * p)
{
// This is 'real width' precision only.
  genie_next_random (p);
  genie_lengthen_real_to_mp (p);
  if (MOID (p) == M_LONG_LONG_REAL) {
    genie_lengthen_mp_to_long_mp (p);
  }
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_csc_mp (NODE_T * p)
{
  C_L_FUNCTION (p, csc_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_acsc_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acsc_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_sec_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sec_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_asec_mp (NODE_T * p)
{
  C_L_FUNCTION (p, asec_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_cot_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cot_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_acot_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acot_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_sindg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sindg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_cosdg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cosdg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_tandg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, tandg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_cotdg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cotdg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_asindg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, asindg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_acosdg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acosdg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_atandg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, atandg_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_acotdg_mp (NODE_T * p)
{
  C_L_FUNCTION (p, acotdg_mp);
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL long arctan2

void genie_atan2dg_mp (NODE_T * p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *y = (MP_T *) STACK_OFFSET (-size);
  MP_T *x = (MP_T *) STACK_OFFSET (-2 * size);
  PRELUDE_ERROR (atan2dg_mp (p, x, y, x, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
  A68_SP -= size;
  MP_STATUS (x) = (MP_T) INIT_MASK;
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_sinpi_mp (NODE_T * p)
{
  C_L_FUNCTION (p, sinpi_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_cospi_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cospi_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_cotpi_mp (NODE_T * p)
{
  C_L_FUNCTION (p, cotpi_mp);
}

//! @brief PROC (LONG REAL) LONG REAL long

void genie_tanpi_mp (NODE_T * p)
{
  C_L_FUNCTION (p, tanpi_mp);
}
