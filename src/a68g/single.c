//! @file single.c
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
#include "a68g-double.h"
#include "a68g-numbers.h"
#include "a68g-stddef.h"

// INT operations.

// OP - = (INT) INT.

A68_MONAD (genie_minus_int, A68_INT, -);

// OP ABS = (INT) INT

void genie_abs_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = ABS (VALUE (j));
}

// OP SIGN = (INT) INT

void genie_sign_int (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = SIGN (VALUE (j));
}

// OP ODD = (INT) BOOL

void genie_odd_int (NODE_T * p)
{
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&j) >= 0 ? VALUE (&j) : -VALUE (&j)) % 2 == 1), A68_BOOL);
}

// OP + = (INT, INT) INT

void genie_add_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  errno = 0;
  VALUE (i) = a68_add_int (VALUE (i), VALUE (j));
  MATH_RTE (p, errno != 0, M_INT, "M overflow");
}

// OP - = (INT, INT) INT

void genie_sub_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  errno = 0;
  VALUE (i) = a68_sub_int (VALUE (i), VALUE (j));
  MATH_RTE (p, errno != 0, M_INT, "M overflow");
}

// OP * = (INT, INT) INT

void genie_mul_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  errno = 0;
  VALUE (i) = a68_mul_int (VALUE (i), VALUE (j));
  MATH_RTE (p, errno != 0, M_INT, "M overflow");
}

// OP OVER = (INT, INT) INT

void genie_over_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  errno = 0;
  VALUE (i) = a68_over_int (VALUE (i), VALUE (j));
  MATH_RTE (p, errno != 0, M_INT, ERROR_DIVISION_BY_ZERO);
}

// OP MOD = (INT, INT) INT

void genie_mod_int (NODE_T * p)
{
  A68_INT *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_INT);
  errno = 0;
  VALUE (i) = a68_mod_int (VALUE (i), VALUE (j));
  MATH_RTE (p, errno != 0, M_INT, ERROR_DIVISION_BY_ZERO);
}

// OP / = (INT, INT) REAL

void genie_div_int (NODE_T * p)
{
  A68_INT i, j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_INT);
  PUSH_VALUE (p, a68_div_int (VALUE (&i), VALUE (&j)), A68_REAL);
  MATH_RTE (p, errno != 0, M_INT, "M division by zero");
}

// OP ** = (INT, INT) INT

void genie_pow_int (NODE_T * p)
{
  A68_INT i, j;
  POP_OBJECT (p, &j, A68_INT);
  PRELUDE_ERROR (VALUE (&j) < 0, p, ERROR_EXPONENT_INVALID, M_INT);
  POP_OBJECT (p, &i, A68_INT);
  PUSH_VALUE (p, a68_m_up_n (VALUE (&i), VALUE (&j)), A68_INT);
  MATH_RTE (p, errno != 0, M_INT, "M overflow");
}

// OP (INT, INT) BOOL.

#define A68_CMP_INT(n, OP)\
void n (NODE_T * p) {\
  A68_INT i, j;\
  POP_OBJECT (p, &j, A68_INT);\
  POP_OBJECT (p, &i, A68_INT);\
  PUSH_VALUE (p, (BOOL_T) (VALUE (&i) OP VALUE (&j)), A68_BOOL);\
  }

A68_CMP_INT (genie_eq_int, ==);
A68_CMP_INT (genie_ne_int, !=);
A68_CMP_INT (genie_lt_int, <);
A68_CMP_INT (genie_gt_int, >);
A68_CMP_INT (genie_le_int, <=);
A68_CMP_INT (genie_ge_int, >=);

// OP +:= = (REF INT, INT) REF INT

void genie_plusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_INT, genie_add_int);
}

// OP -:= = (REF INT, INT) REF INT

void genie_minusab_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_INT, genie_sub_int);
}

// OP *:= = (REF INT, INT) REF INT

void genie_timesab_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_INT, genie_mul_int);
}

// OP %:= = (REF INT, INT) REF INT

void genie_overab_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_INT, genie_over_int);
}

// OP %*:= = (REF INT, INT) REF INT

void genie_modab_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_INT, genie_mod_int);
}

// REAL operations.

// OP - = (REAL) REAL.

A68_MONAD (genie_minus_real, A68_REAL, -);

// OP ABS = (REAL) REAL

void genie_abs_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  VALUE (x) = ABS (VALUE (x));
}

// OP ROUND = (REAL) INT

void genie_round_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PRELUDE_ERROR (VALUE (&x) < -(REAL_T) A68_MAX_INT || VALUE (&x) > (REAL_T) A68_MAX_INT, p, ERROR_OUT_OF_BOUNDS, M_INT);
  PUSH_VALUE (p, a68_round (VALUE (&x)), A68_INT);
}

// OP ENTIER = (REAL) INT

void genie_entier_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PRELUDE_ERROR (VALUE (&x) < -(REAL_T) A68_MAX_INT || VALUE (&x) > (REAL_T) A68_MAX_INT, p, ERROR_OUT_OF_BOUNDS, M_INT);
  PUSH_VALUE (p, (INT_T) floor (VALUE (&x)), A68_INT);
}

// OP SIGN = (REAL) INT

void genie_sign_real (NODE_T * p)
{
  A68_REAL x;
  POP_OBJECT (p, &x, A68_REAL);
  PUSH_VALUE (p, SIGN (VALUE (&x)), A68_INT);
}

// OP + = (REAL, REAL) REAL

void genie_add_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) += VALUE (y);
  CHECK_REAL (p, VALUE (x));
}

// OP - = (REAL, REAL) REAL

void genie_sub_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) -= VALUE (y);
  CHECK_REAL (p, VALUE (x));
}

// OP * = (REAL, REAL) REAL

void genie_mul_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  VALUE (x) *= VALUE (y);
  CHECK_REAL (p, VALUE (x));
}

// OP / = (REAL, REAL) REAL

void genie_div_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  PRELUDE_ERROR (VALUE (y) == 0.0, p, ERROR_DIVISION_BY_ZERO, M_REAL);
  VALUE (x) /= VALUE (y);
}

// OP ** = (REAL, INT) REAL

void genie_pow_real_int (NODE_T * p)
{
  A68_INT j;
  A68_REAL x;
  REAL_T z;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &x, A68_REAL);
  z = a68_x_up_n (VALUE (&x), VALUE (&j));
  CHECK_REAL (p, z);
  PUSH_VALUE (p, z, A68_REAL);
}

// OP ** = (REAL, REAL) REAL

void genie_pow_real (NODE_T * p)
{
  A68_REAL x, y;
  REAL_T z = 0;
  POP_OBJECT (p, &y, A68_REAL);
  POP_OBJECT (p, &x, A68_REAL);
  errno = 0;
  z = a68_x_up_y (VALUE (&x), VALUE (&y));
  MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);
  PUSH_VALUE (p, z, A68_REAL);
}

// OP (REAL, REAL) BOOL.

#define A68_CMP_REAL(n, OP)\
void n (NODE_T * p) {\
  A68_REAL i, j;\
  POP_OBJECT (p, &j, A68_REAL);\
  POP_OBJECT (p, &i, A68_REAL);\
  PUSH_VALUE (p, (BOOL_T) (VALUE (&i) OP VALUE (&j)), A68_BOOL);\
  }

A68_CMP_REAL (genie_eq_real, ==);
A68_CMP_REAL (genie_ne_real, !=);
A68_CMP_REAL (genie_lt_real, <);
A68_CMP_REAL (genie_gt_real, >);
A68_CMP_REAL (genie_le_real, <=);
A68_CMP_REAL (genie_ge_real, >=);

// OP +:= = (REF REAL, REAL) REF REAL

void genie_plusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_REAL, genie_add_real);
}

// OP -:= = (REF REAL, REAL) REF REAL

void genie_minusab_real (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_REAL, genie_sub_real);
}

// OP *:= = (REF REAL, REAL) REF REAL

void genie_timesab_real (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_REAL, genie_mul_real);
}

// OP /:= = (REF REAL, REAL) REF REAL

void genie_divab_real (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_REAL, genie_div_real);
}

// @brief PROC (INT) VOID first random

void genie_first_random (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  init_rng ((unsigned) VALUE (&i));
}

// @brief PROC REAL next random

void genie_next_random (NODE_T * p)
{
  PUSH_VALUE (p, unif_rand (), A68_REAL);
}

// @brief PROC REAL rnd

void genie_next_rnd (NODE_T * p)
{
  PUSH_VALUE (p, 2 * unif_rand () - 1, A68_REAL);
}

// BITS operations.

// BITS max bits

void genie_max_bits (NODE_T * p)
{
  PUSH_VALUE (p, A68_MAX_BITS, A68_BITS);
}

// OP NOT = (BITS) BITS.
A68_MONAD (genie_not_bits, A68_BITS, ~);

// OP AND = (BITS, BITS) BITS

void genie_and_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) & VALUE (j);
}

// OP OR = (BITS, BITS) BITS

void genie_or_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) | VALUE (j);
}

// OP XOR = (BITS, BITS) BITS

void genie_xor_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  VALUE (i) = VALUE (i) ^ VALUE (j);
}

// OP + = (BITS, BITS) BITS

void genie_add_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  CHECK_BITS_ADDITION (p, VALUE (i), VALUE (j));
  VALUE (i) = VALUE (i) + VALUE (j);
}

// OP - = (BITS, BITS) BITS

void genie_sub_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  CHECK_BITS_SUBTRACTION (p, VALUE (i), VALUE (j));
  VALUE (i) = VALUE (i) - VALUE (j);
}

// OP * = (BITS, BITS) BITS

void genie_times_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  CHECK_BITS_MULTIPLICATION (p, VALUE (i), VALUE (j));
  VALUE (i) = VALUE (i) * VALUE (j);
}

// OP OVER = (BITS, BITS) BITS

void genie_over_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  PRELUDE_ERROR (VALUE (j) == 0, p, ERROR_DIVISION_BY_ZERO, M_BITS);
  VALUE (i) = VALUE (i) / VALUE (j);
}

// OP MOD = (BITS, BITS) BITS

void genie_mod_bits (NODE_T * p)
{
  A68_BITS *i, *j;
  POP_OPERAND_ADDRESSES (p, i, j, A68_BITS);
  PRELUDE_ERROR (VALUE (j) == 0, p, ERROR_DIVISION_BY_ZERO, M_BITS);
  VALUE (i) = VALUE (i) % VALUE (j);
}

// OP = = (BITS, BITS) BOOL.

#define A68_CMP_BITS(n, OP)\
void n (NODE_T * p) {\
  A68_BITS i, j;\
  POP_OBJECT (p, &j, A68_BITS);\
  POP_OBJECT (p, &i, A68_BITS);\
  PUSH_VALUE (p, (BOOL_T) ((UNSIGNED_T) VALUE (&i) OP (UNSIGNED_T) VALUE (&j)), A68_BOOL);\
  }

A68_CMP_BITS (genie_eq_bits, ==);
A68_CMP_BITS (genie_ne_bits, !=);

// OP <= = (BITS, BITS) BOOL

void genie_le_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&j)), A68_BOOL);
}

// OP >= = (BITS, BITS) BOOL

void genie_ge_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&i)), A68_BOOL);
}

#if (A68_LEVEL >= 3)

// OP < = (BITS, BITS) BOOL

void genie_lt_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  if (VALUE (&i) == VALUE (&j)) {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  } else {
    PUSH_VALUE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&j)), A68_BOOL);
  }
}

// OP >= = (BITS, BITS) BOOL

void genie_gt_bits (NODE_T * p)
{
  A68_BITS i, j;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_BITS);
  if (VALUE (&i) == VALUE (&j)) {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  } else {
    PUSH_VALUE (p, (BOOL_T) ((VALUE (&i) | VALUE (&j)) == VALUE (&i)), A68_BOOL);
  }
}

#endif

// OP SHL = (BITS, INT) BITS

void genie_shl_bits (NODE_T * p)
{
  A68_BITS i;
  A68_INT j;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_BITS);
  if (VALUE (&j) >= 0) {
    int k;
    UNSIGNED_T z = VALUE (&i);
    for (k = 0; k < VALUE (&j); k++) {
      PRELUDE_ERROR (!MODULAR_MATH (p) && (z & D_SIGN), p, ERROR_MATH, M_BITS);
      z = z << 1;
    }
    PUSH_VALUE (p, z, A68_BITS);
  } else {
    PUSH_VALUE (p, VALUE (&i) >> -VALUE (&j), A68_BITS);
  }
}

// OP SHR = (BITS, INT) BITS

void genie_shr_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_shl_bits (p);           // Conform RR
}

// OP ROL = (BITS, INT) BITS

void genie_rol_bits (NODE_T * p)
{
  A68_BITS i;
  A68_INT j;
  int k, n;
  UNSIGNED_T w;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_BITS);
  CHECK_INT_SHORTEN (p, VALUE (&j));
  w = VALUE (&i);
  n = VALUE (&j);
  if (n >= 0) {
    for (k = 0; k < n; k++) {
      UNSIGNED_T carry = (w & D_SIGN ? 0x1 : 0x0);
      w = (w << 1) | carry;
    }
  } else {
    n = -n;
    for (k = 0; k < n; k++) {
      UNSIGNED_T carry = (w & 0x1 ? D_SIGN : 0x0);
      w = (w >> 1) | carry;
    }
  }
  PUSH_VALUE (p, w, A68_BITS);
}

// OP ROR = (BITS, INT) BITS

void genie_ror_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_rol_bits (p);
}

// OP ELEM = (INT, BITS) BOOL

void genie_elem_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  UNSIGNED_T mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&j) & mask) != 0 ? A68_TRUE : A68_FALSE), A68_BOOL);
}

// OP SET = (INT, BITS) BITS

void genie_set_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  UNSIGNED_T mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_VALUE (p, VALUE (&j) | mask, A68_BITS);
}

// OP CLEAR = (INT, BITS) BITS

void genie_clear_bits (NODE_T * p)
{
  A68_BITS j;
  A68_INT i;
  int n;
  UNSIGNED_T mask = 0x1;
  POP_OBJECT (p, &j, A68_BITS);
  POP_OBJECT (p, &i, A68_INT);
  PRELUDE_ERROR (VALUE (&i) < 1 || VALUE (&i) > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_VALUE (p, VALUE (&j) & ~mask, A68_BITS);
}

// OP ABS = (BITS) INT

void genie_abs_bits (NODE_T * p)
{
  A68_BITS i;
  POP_OBJECT (p, &i, A68_BITS);
  PUSH_VALUE (p, (INT_T) (VALUE (&i)), A68_INT);
}

// OP BIN = (INT) BITS

void genie_bin_int (NODE_T * p)
{
  A68_INT i;
  POP_OBJECT (p, &i, A68_INT);
  if (!MODULAR_MATH (p) && VALUE (&i) < 0) {
// RR does not convert negative numbers.
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_BITS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_VALUE (p, (UNSIGNED_T) (VALUE (&i)), A68_BITS);
}

// @brief PROC ([] BOOL) BITS bits pack

void genie_bits_pack (NODE_T * p)
{
  A68_REF z;
  A68_BITS b;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  BYTE_T *base;
  int size, k;
  UNSIGNED_T bit;
  POP_REF (p, &z);
  CHECK_REF (p, z, M_ROW_BOOL);
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  PRELUDE_ERROR (size < 0 || size > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_ROW_BOOL);
  VALUE (&b) = 0x0;
  if (ROW_SIZE (tup) > 0) {
    base = DEREF (BYTE_T, &ARRAY (arr));
    bit = 0x1;
    for (k = UPB (tup); k >= LWB (tup); k--) {
      int addr = INDEX_1_DIM (arr, tup, k);
      A68_BOOL *boo = (A68_BOOL *) & (base[addr]);
      CHECK_INIT (p, INITIALISED (boo), M_BOOL);
      if (VALUE (boo)) {
        VALUE (&b) |= bit;
      }
      bit <<= 1;
    }
  }
  STATUS (&b) = INIT_MASK;
  PUSH_OBJECT (p, b, A68_BITS);
}

// @brief PROC (REAL) REAL sqrt

void genie_sqrt_real (NODE_T * p)
{
  C_FUNCTION (p, sqrt);
}

// @brief PROC (REAL) REAL curt

void genie_curt_real (NODE_T * p)
{
  C_FUNCTION (p, cbrt);
}

// @brief PROC (REAL) REAL exp

void genie_exp_real (NODE_T * p)
{
  A68_REAL *x;
  POP_OPERAND_ADDRESS (p, x, A68_REAL);
  if (VALUE (x) > LOG_DBL_MAX) {
    errno = EDOM;
  } else if (VALUE (x) < LOG_DBL_MIN) {
    errno = EDOM;
  } else {
    errno = 0;
    VALUE (x) = exp (VALUE (x));
  }
  MATH_RTE (p, errno != 0, M_REAL, NO_TEXT);
}

// @brief PROC (REAL) REAL ln

void genie_ln_real (NODE_T * p)
{
  C_FUNCTION (p, a68_ln);
}

// @brief PROC (REAL) REAL ln1p

void genie_ln1p_real (NODE_T * p)
{
  C_FUNCTION (p, a68_ln1p);
}

// @brief PROC (REAL) REAL log

void genie_log_real (NODE_T * p)
{
  C_FUNCTION (p, log10);
}

// @brief PROC (REAL) REAL sin

void genie_sin_real (NODE_T * p)
{
  C_FUNCTION (p, sin);
}

// @brief PROC (REAL) REAL arcsin

void genie_asin_real (NODE_T * p)
{
  C_FUNCTION (p, asin);
}

// @brief PROC (REAL) REAL cos

void genie_cos_real (NODE_T * p)
{
  C_FUNCTION (p, cos);
}

// @brief PROC (REAL) REAL arccos

void genie_acos_real (NODE_T * p)
{
  C_FUNCTION (p, acos);
}

// @brief PROC (REAL) REAL tan

void genie_tan_real (NODE_T * p)
{
  C_FUNCTION (p, tan);
}

// @brief PROC (REAL) REAL csc 

void genie_csc_real (NODE_T * p)
{
  C_FUNCTION (p, a68_csc);
}

// @brief PROC (REAL) REAL acsc

void genie_acsc_real (NODE_T * p)
{
  C_FUNCTION (p, a68_acsc);
}

// @brief PROC (REAL) REAL sec 

void genie_sec_real (NODE_T * p)
{
  C_FUNCTION (p, a68_sec);
}

// @brief PROC (REAL) REAL asec

void genie_asec_real (NODE_T * p)
{
  C_FUNCTION (p, a68_asec);
}

// @brief PROC (REAL) REAL cot 

void genie_cot_real (NODE_T * p)
{
  C_FUNCTION (p, a68_cot);
}

// @brief PROC (REAL) REAL acot

void genie_acot_real (NODE_T * p)
{
  C_FUNCTION (p, a68_acot);
}

// @brief PROC (REAL) REAL arctan

void genie_atan_real (NODE_T * p)
{
  C_FUNCTION (p, atan);
}

// @brief PROC (REAL, REAL) REAL arctan2

void genie_atan2_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  errno = 0;
  PRELUDE_ERROR (VALUE (x) == 0.0 && VALUE (y) == 0.0, p, ERROR_INVALID_ARGUMENT, M_LONG_REAL);
  VALUE (x) = a68_atan2 (VALUE (y), VALUE (x));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

// @brief PROC (REAL) REAL sindg

void genie_sindg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_sindg);
}

// @brief PROC (REAL) REAL arcsindg

void genie_asindg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_asindg);
}

// @brief PROC (REAL) REAL cosdg

void genie_cosdg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_cosdg);
}

// @brief PROC (REAL) REAL arccosdg

void genie_acosdg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_acosdg);
}

// @brief PROC (REAL) REAL tandg

void genie_tandg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_tandg);
}

// @brief PROC (REAL) REAL arctandg

void genie_atandg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_atandg);
}

// @brief PROC (REAL) REAL cotdg 

void genie_cotdg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_cotdg);
}

// @brief PROC (REAL) REAL acotdg

void genie_acotdg_real (NODE_T * p)
{
  C_FUNCTION (p, a68_acotdg);
}

// @brief PROC (REAL, REAL) REAL arctan2dg

void genie_atan2dg_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  errno = 0;
  PRELUDE_ERROR (VALUE (x) == 0.0 && VALUE (y) == 0.0, p, ERROR_INVALID_ARGUMENT, M_LONG_REAL);
  VALUE (x) = CONST_180_OVER_PI * a68_atan2 (VALUE (y), VALUE (x));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

// @brief PROC (REAL) REAL sinpi

void genie_sinpi_real (NODE_T * p)
{
  C_FUNCTION (p, a68_sinpi);
}

// @brief PROC (REAL) REAL cospi

void genie_cospi_real (NODE_T * p)
{
  C_FUNCTION (p, a68_cospi);
}

// @brief PROC (REAL) REAL tanpi

void genie_tanpi_real (NODE_T * p)
{
  C_FUNCTION (p, a68_tanpi);
}

// @brief PROC (REAL) REAL cotpi 

void genie_cotpi_real (NODE_T * p)
{
  C_FUNCTION (p, a68_cotpi);
}

// @brief PROC (REAL) REAL sinh

void genie_sinh_real (NODE_T * p)
{
  C_FUNCTION (p, sinh);
}

// @brief PROC (REAL) REAL cosh

void genie_cosh_real (NODE_T * p)
{
  C_FUNCTION (p, cosh);
}

// @brief PROC (REAL) REAL tanh

void genie_tanh_real (NODE_T * p)
{
  C_FUNCTION (p, tanh);
}

// @brief PROC (REAL) REAL asinh

void genie_asinh_real (NODE_T * p)
{
  C_FUNCTION (p, a68_asinh);
}

// @brief PROC (REAL) REAL acosh

void genie_acosh_real (NODE_T * p)
{
  C_FUNCTION (p, a68_acosh);
}

// @brief PROC (REAL) REAL atanh

void genie_atanh_real (NODE_T * p)
{
  C_FUNCTION (p, a68_atanh);
}

// @brief PROC (REAL) REAL erf

void genie_erf_real (NODE_T * p)
{
  C_FUNCTION (p, erf);
}

// @brief PROC (REAL) REAL inverf

void genie_inverf_real (NODE_T * p)
{
  C_FUNCTION (p, a68_inverf);
}

// @brief PROC (REAL) REAL erfc

void genie_erfc_real (NODE_T * p)
{
  C_FUNCTION (p, erfc);
}

// @brief PROC (REAL) REAL inverfc

void genie_inverfc_real (NODE_T * p)
{
  C_FUNCTION (p, a68_inverfc);
}

// @brief PROC (REAL) REAL gamma

void genie_gamma_real (NODE_T * p)
{
  C_FUNCTION (p, tgamma);
}

// @brief PROC (REAL) REAL ln gamma

void genie_ln_gamma_real (NODE_T * p)
{
  C_FUNCTION (p, lgamma);
}

// @brief PROC (REAL, REAL) REAL beta

void genie_beta_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  errno = 0;
  VALUE (x) = a68_beta (VALUE (x), VALUE (y));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

// @brief PROC (REAL, REAL) REAL ln beta

void genie_ln_beta_real (NODE_T * p)
{
  A68_REAL *x, *y;
  POP_OPERAND_ADDRESSES (p, x, y, A68_REAL);
  errno = 0;
  VALUE (x) = a68_ln_beta (VALUE (x), VALUE (y));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

// @brief PROC (REAL, REAL, REAL) REAL cf beta inc

void genie_beta_inc_cf_real (NODE_T * p)
{
  A68_REAL *s, *t, *x;
  POP_3_OPERAND_ADDRESSES (p, s, t, x, A68_REAL);
  VALUE (s) = a68_beta_inc (VALUE (s), VALUE (t), VALUE (x));
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
}

// @brief PROC (REAL, REAL, REAL) REAL lj e 12 6

void genie_lj_e_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  REAL_T u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  PRELUDE_ERROR (VALUE (r) == 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 4.0 * VALUE (e) * u6 * (u6 - 1.0);
}

// @brief PROC (REAL, REAL, REAL) REAL lj f 12 6

void genie_lj_f_12_6 (NODE_T * p)
{
  A68_REAL *e, *s, *r;
  REAL_T u, u2, u6;
  POP_3_OPERAND_ADDRESSES (p, e, s, r, A68_REAL);
  PRELUDE_ERROR (VALUE (r) == 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
  u = (VALUE (s) / VALUE (r));
  u2 = u * u;
  u6 = u2 * u2 * u2;
  VALUE (e) = 24.0 * VALUE (e) * u * u6 * (1.0 - 2.0 * u6);
}

// This file also contains Algol68G's standard environ for complex numbers.
// Some of the LONG operations are generic for LONG and LONG LONG.
// 
// Some routines are based on
//   GNU Scientific Library
//   Abramowitz and Stegun.

// OP +* = (REAL, REAL) COMPLEX

void genie_i_complex (NODE_T * p)
{
// This function must exist so the code generator recognises it!
  (void) p;
}

// OP +* = (INT, INT) COMPLEX

void genie_i_int_complex (NODE_T * p)
{
  A68_INT re, im;
  POP_OBJECT (p, &im, A68_INT);
  POP_OBJECT (p, &re, A68_INT);
  PUSH_VALUE (p, (REAL_T) VALUE (&re), A68_REAL);
  PUSH_VALUE (p, (REAL_T) VALUE (&im), A68_REAL);
}

// OP RE = (COMPLEX) REAL

void genie_re_complex (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, SIZE (M_REAL));
}

// OP IM = (COMPLEX) REAL

void genie_im_complex (NODE_T * p)
{
  A68_REAL im;
  POP_OBJECT (p, &im, A68_REAL);
  *(A68_REAL *) (STACK_OFFSET (-SIZE (M_REAL))) = im;
}

// OP - = (COMPLEX) COMPLEX

void genie_minus_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x;
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE (M_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE (M_REAL)));
  VALUE (im_x) = -VALUE (im_x);
  VALUE (re_x) = -VALUE (re_x);
  (void) p;
}

// ABS = (COMPLEX) REAL

void genie_abs_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_VALUE (p, a68_hypot (VALUE (&re_x), VALUE (&im_x)), A68_REAL);
}

// OP ARG = (COMPLEX) REAL

void genie_arg_complex (NODE_T * p)
{
  A68_REAL re_x, im_x;
  POP_COMPLEX (p, &re_x, &im_x);
  PRELUDE_ERROR (VALUE (&re_x) == 0.0 && VALUE (&im_x) == 0.0, p, ERROR_INVALID_ARGUMENT, M_COMPLEX);
  PUSH_VALUE (p, atan2 (VALUE (&im_x), VALUE (&re_x)), A68_REAL);
}

// OP CONJ = (COMPLEX) COMPLEX

void genie_conj_complex (NODE_T * p)
{
  A68_REAL *im;
  POP_OPERAND_ADDRESS (p, im, A68_REAL);
  VALUE (im) = -VALUE (im);
}

// OP + = (COMPLEX, COMPLEX) COMPLEX

void genie_add_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE (M_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE (M_REAL)));
  VALUE (im_x) += VALUE (&im_y);
  VALUE (re_x) += VALUE (&re_y);
  CHECK_COMPLEX (p, VALUE (re_x), VALUE (im_x));
}

// OP - = (COMPLEX, COMPLEX) COMPLEX

void genie_sub_complex (NODE_T * p)
{
  A68_REAL *re_x, *im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  im_x = (A68_REAL *) (STACK_OFFSET (-SIZE (M_REAL)));
  re_x = (A68_REAL *) (STACK_OFFSET (-2 * SIZE (M_REAL)));
  VALUE (im_x) -= VALUE (&im_y);
  VALUE (re_x) -= VALUE (&re_y);
  CHECK_COMPLEX (p, VALUE (re_x), VALUE (im_x));
}

// OP * = (COMPLEX, COMPLEX) COMPLEX

void genie_mul_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  REAL_T re, im;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  re = VALUE (&re_x) * VALUE (&re_y) - VALUE (&im_x) * VALUE (&im_y);
  im = VALUE (&im_x) * VALUE (&re_y) + VALUE (&re_x) * VALUE (&im_y);
  CHECK_COMPLEX (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

// OP / = (COMPLEX, COMPLEX) COMPLEX

void genie_div_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  REAL_T re = 0.0, im = 0.0;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
#if !defined (HAVE_IEEE_754)
  PRELUDE_ERROR (VALUE (&re_y) == 0.0 && VALUE (&im_y) == 0.0, p, ERROR_DIVISION_BY_ZERO, M_COMPLEX);
#endif
  if (ABS (VALUE (&re_y)) >= ABS (VALUE (&im_y))) {
    REAL_T r = VALUE (&im_y) / VALUE (&re_y), den = VALUE (&re_y) + r * VALUE (&im_y);
    re = (VALUE (&re_x) + r * VALUE (&im_x)) / den;
    im = (VALUE (&im_x) - r * VALUE (&re_x)) / den;
  } else {
    REAL_T r = VALUE (&re_y) / VALUE (&im_y), den = VALUE (&im_y) + r * VALUE (&re_y);
    re = (VALUE (&re_x) * r + VALUE (&im_x)) / den;
    im = (VALUE (&im_x) * r - VALUE (&re_x)) / den;
  }
  CHECK_COMPLEX (p, re, im);
  PUSH_COMPLEX (p, re, im);
}

// OP ** = (COMPLEX, INT) COMPLEX

void genie_pow_complex_int (NODE_T * p)
{
  A68_REAL re_x, im_x;
  REAL_T re_y, im_y, re_z, im_z, rea;
  A68_INT j;
  INT_T expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  POP_COMPLEX (p, &re_x, &im_x);
  re_z = 1.0;
  im_z = 0.0;
  re_y = VALUE (&re_x);
  im_y = VALUE (&im_x);
  expo = 1;
  negative = (BOOL_T) (VALUE (&j) < 0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((UNSIGNED_T) expo <= (UNSIGNED_T) (VALUE (&j))) {
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
  CHECK_COMPLEX (p, re_z, im_z);
  if (negative) {
    PUSH_VALUE (p, 1.0, A68_REAL);
    PUSH_VALUE (p, 0.0, A68_REAL);
    PUSH_VALUE (p, re_z, A68_REAL);
    PUSH_VALUE (p, im_z, A68_REAL);
    genie_div_complex (p);
  } else {
    PUSH_VALUE (p, re_z, A68_REAL);
    PUSH_VALUE (p, im_z, A68_REAL);
  }
}

// OP = = (COMPLEX, COMPLEX) BOOL

void genie_eq_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&re_x) == VALUE (&re_y)) && (VALUE (&im_x) == VALUE (&im_y))), A68_BOOL);
}

// OP /= = (COMPLEX, COMPLEX) BOOL

void genie_ne_complex (NODE_T * p)
{
  A68_REAL re_x, im_x, re_y, im_y;
  POP_COMPLEX (p, &re_y, &im_y);
  POP_COMPLEX (p, &re_x, &im_x);
  PUSH_VALUE (p, (BOOL_T) ! ((VALUE (&re_x) == VALUE (&re_y)) && (VALUE (&im_x) == VALUE (&im_y))), A68_BOOL);
}

// OP +:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_plusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_COMPLEX, genie_add_complex);
}

// OP -:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_minusab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_COMPLEX, genie_sub_complex);
}

// OP *:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_timesab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_COMPLEX, genie_mul_complex);
}

// OP /:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_divab_complex (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_COMPLEX, genie_div_complex);
}

#define C_C_FUNCTION(p, f)\
  A68_REAL re, im;\
  COMPLEX_T z;\
  POP_OBJECT (p, &im, A68_REAL);\
  POP_OBJECT (p, &re, A68_REAL);\
  errno = 0;\
  z = VALUE (&re) + VALUE (&im) * _Complex_I;\
  z = f (z);\
  PUSH_VALUE (p, (REAL_T) creal (z), A68_REAL);\
  PUSH_VALUE (p, (REAL_T) cimag (z), A68_REAL);\
  MATH_RTE (p, errno != 0, M_COMPLEX, NO_TEXT);

// @brief PROC (COMPLEX) COMPLEX csqrt

void genie_sqrt_complex (NODE_T * p)
{
  C_C_FUNCTION (p, csqrt);
}

// @brief PROC (COMPLEX) COMPLEX cexp

void genie_exp_complex (NODE_T * p)
{
  C_C_FUNCTION (p, cexp);
}

// @brief PROC (COMPLEX) COMPLEX cln

void genie_ln_complex (NODE_T * p)
{
  C_C_FUNCTION (p, clog);
}

// @brief PROC (COMPLEX) COMPLEX csin

void genie_sin_complex (NODE_T * p)
{
  C_C_FUNCTION (p, csin);
}

// @brief PROC (COMPLEX) COMPLEX ccos

void genie_cos_complex (NODE_T * p)
{
  C_C_FUNCTION (p, ccos);
}

// @brief PROC (COMPLEX) COMPLEX ctan

void genie_tan_complex (NODE_T * p)
{
  C_C_FUNCTION (p, ctan);
}

// @brief PROC carcsin= (COMPLEX) COMPLEX

void genie_asin_complex (NODE_T * p)
{
  C_C_FUNCTION (p, casin);
}

// @brief PROC (COMPLEX) COMPLEX carccos

void genie_acos_complex (NODE_T * p)
{
  C_C_FUNCTION (p, cacos);
}

// @brief PROC (COMPLEX) COMPLEX carctan

void genie_atan_complex (NODE_T * p)
{
  C_C_FUNCTION (p, catan);
}

// @brief PROC (COMPLEX) COMPLEX csinh

void genie_sinh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, csinh);
}

// @brief PROC (COMPLEX) COMPLEX ccosh

void genie_cosh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, ccosh);
}

// @brief PROC (COMPLEX) COMPLEX ctanh

void genie_tanh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, ctanh);
}

// @brief PROC (COMPLEX) COMPLEX carcsinh

void genie_asinh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, casinh);
}

// @brief PROC (COMPLEX) COMPLEX carccosh

void genie_acosh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, cacosh);
}

// @brief PROC (COMPLEX) COMPLEX carctanh

void genie_atanh_complex (NODE_T * p)
{
  C_C_FUNCTION (p, catanh);
}

#define C_C_INLINE(z, x, f)\
  COMPLEX_T u = RE (x) + IM (x) * _Complex_I;\
  COMPLEX_T v = f (u);\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = creal (v);\
  IM (z) = cimag (v);\

//! @brief PROC (COMPLEX) COMPLEX csqrt

void a68_sqrt_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, csqrt);
}

//! @brief PROC (COMPLEX) COMPLEX cexp

void a68_exp_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, cexp);
}

//! @brief PROC (COMPLEX) COMPLEX cln

void a68_ln_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, clog);
}

//! @brief PROC (COMPLEX) COMPLEX csin

void a68_sin_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, csin);
}

//! @brief PROC (COMPLEX) COMPLEX ccos

void a68_cos_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, ccos);
}

//! @brief PROC (COMPLEX) COMPLEX ctan

void a68_tan_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, ctan);
}

//! @brief PROC (COMPLEX) COMPLEX casin

void a68_asin_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, casin);
}

//! @brief PROC (COMPLEX) COMPLEX cacos

void a68_acos_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, cacos);
}

//! @brief PROC (COMPLEX) COMPLEX catan

void a68_atan_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, catan);
}

//! @brief PROC (COMPLEX) COMPLEX csinh

void a68_sinh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, csinh);
}

//! @brief PROC (COMPLEX) COMPLEX ccosh

void a68_cosh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, ccosh);
}

//! @brief PROC (COMPLEX) COMPLEX ctanh

void a68_tanh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, ctanh);
}

//! @brief PROC (COMPLEX) COMPLEX casinh

void a68_asinh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, casinh);
}

//! @brief PROC (COMPLEX) COMPLEX cacosh

void a68_acosh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, cacosh);
}

//! @brief PROC (COMPLEX) COMPLEX catanh

void a68_atanh_complex (A68_REAL * z, A68_REAL * x)
{
  C_C_INLINE (z, x, catanh);
}

//! @brief PROC (INT, INT) REAL choose

void genie_fact_real (NODE_T * p)
{
  A68_INT n;
  POP_OBJECT (p, &n, A68_INT);
  PUSH_VALUE (p, a68_fact (VALUE (&n)), A68_REAL);
  MATH_RTE (p, errno != 0, M_INT, NO_TEXT);
}

//! @brief PROC (INT, INT) REAL ln fact

void genie_ln_fact_real (NODE_T * p)
{
  A68_INT n;
  POP_OBJECT (p, &n, A68_INT);
  PUSH_VALUE (p, a68_ln_fact (VALUE (&n)), A68_REAL);
  MATH_RTE (p, errno != 0, M_INT, NO_TEXT);
}

void genie_choose_real (NODE_T * p)
{
  A68_INT n, m;
  POP_OBJECT (p, &m, A68_INT);
  POP_OBJECT (p, &n, A68_INT);
  PUSH_VALUE (p, a68_choose (VALUE (&n), VALUE (&m)), A68_REAL);
  MATH_RTE (p, errno != 0, M_INT, NO_TEXT);
}

//! @brief PROC (INT, INT) REAL ln choose

void genie_ln_choose_real (NODE_T * p)
{
  A68_INT n, m;
  POP_OBJECT (p, &m, A68_INT);
  POP_OBJECT (p, &n, A68_INT);
  PUSH_VALUE (p, a68_ln_choose (VALUE (&n), VALUE (&m)), A68_REAL);
  MATH_RTE (p, errno != 0, M_INT, NO_TEXT);
}

// OP / = (COMPLEX, COMPLEX) COMPLEX

void a68_div_complex (A68_REAL * z, A68_REAL * x, A68_REAL * y)
{
  if (RE (y) == 0 && IM (y) == 0) {
    STATUS_RE (z) = INIT_MASK;
    STATUS_IM (z) = INIT_MASK;
    RE (z) = 0.0;
    IM (z) = 0.0;
    errno = EDOM;
  } else if (fabs (RE (y)) >= fabs (IM (y))) {
    REAL_T r = IM (y) / RE (y), den = RE (y) + r * IM (y);
    RE (z) = (RE (x) + r * IM (x)) / den;
    IM (z) = (IM (x) - r * RE (x)) / den;
  } else {
    REAL_T r = RE (y) / IM (y), den = IM (y) + r * RE (y);
    STATUS_RE (z) = INIT_MASK;
    STATUS_IM (z) = INIT_MASK;
    RE (z) = (RE (x) * r + IM (x)) / den;
    IM (z) = (IM (x) * r - RE (x)) / den;
  }
}
