//! @file double.c
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
//! LONG INT, LONG REAL and LONG BITS routines.

#include "a68g.h"

#if (A68_LEVEL >= 3)

#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-transput.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-lib.h"
#include "a68g-numbers.h"

// 128-bit REAL*16 stuff.

#define RADIX (65536)
#define RADIX_Q (65536.0q)
#define CONST_2_UP_112_Q (5192296858534827628530496329220096.0q)
#define DOUBLE_DIGITS MANT_DIGS (FLT128_MANT_DIG)

#define IS_ZERO(u) (HW (u) == 0 && LW (u) == 0)
#define EQ(u, v) (HW (u) == HW (v) && LW (u) == LW (v))
#define GT(u, v) (HW (u) != HW (v) ? HW (u) > HW (v) : LW (u) > LW (v))
#define GE(u, v) (HW (u) != HW (v) ? HW (u) >= HW (v) : LW (u) >= LW (v))

DOUBLE_NUM_T double_ssub (NODE_T *, DOUBLE_NUM_T, DOUBLE_NUM_T);

void m64to128 (DOUBLE_NUM_T * w, UNSIGNED_T u, UNSIGNED_T v)
{
// Knuth's 'M' algorithm.
#define M (0xffffffff)
#define N 32
  UNSIGNED_T k, t, w1, w2, w3;
  UNSIGNED_T hu = u >> N, lu = u & M, hv = v >> N, lv = v & M;
  t = lu * lv;
  w3 = t & M;
  k = t >> N;
  t = hu * lv + k;
  w2 = t & M;
  w1 = t >> N;
  t = lu * hv + w2;
  k = t >> N;
  HW (*w) = hu * hv + w1 + k;
  LW (*w) = (t << N) + w3;
#undef M
#undef N
}

void m128to128 (NODE_T * p, MOID_T * m, DOUBLE_NUM_T * w, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
// Knuth's 'M' algorithm.
  DOUBLE_NUM_T w1, w2, w3;
  DOUBLE_NUM_T k, t, h;
  UNSIGNED_T hu = HW (u), lu = LW (u), hv = HW (v), lv = LW (v);
  if (lu == 0 || lv == 0) {
    set_lw (t, 0);
  } else {
    m64to128 (&t, lu, lv);
  }
  set_lw (w3, LW (t));
  set_lw (k, HW (t));
  if (hu == 0 || lv == 0) {
    set_lw (t, 0);
  } else {
    m64to128 (&t, hu, lv);
  }
  add_double (p, m, t, t, k);
  set_lw (w2, LW (t));
  set_lw (w1, HW (t));
  if (lu == 0 || hv == 0) {
    set_lw (t, 0);
  } else {
    m64to128 (&t, lu, hv);
  }
  add_double (p, m, t, t, w2);
  set_lw (k, HW (t));
  if (hu == 0 || hv == 0) {
    set_lw (h, 0);
  } else {
    m64to128 (&h, hu, hv);
  }
  add_double (p, m, h, h, w1);
  add_double (p, m, h, h, k);
  set_hw (*w, LW (t));
  add_double (p, m, *w, *w, w3);
  PRELUDE_ERROR (MODCHK (p, m, HW (h) != 0 || LW (h) != 0), p, ERROR_MATH, M_LONG_INT)
}

DOUBLE_NUM_T double_udiv (NODE_T * p, MOID_T * m, DOUBLE_NUM_T n, DOUBLE_NUM_T d, int mode)
{
// A bit naive long division.
  int k;
  UNSIGNED_T carry;
  DOUBLE_NUM_T q, r;
// Special cases.
  PRELUDE_ERROR (IS_ZERO (d), p, ERROR_DIVISION_BY_ZERO, M_LONG_INT);
  if (IS_ZERO (n)) {
    if (mode == 0) {
      set_lw (q, 0);
      return q;
    } else {
      set_lw (r, 0);
      return r;
    }
  }
// Would n and d be random, then ~50% of the divisions is trivial.
  if (EQ (n, d)) {
    if (mode == 0) {
      set_lw (q, 1);
      return q;
    } else {
      set_lw (r, 0);
      return r;
    }
  } else if (GT (d, n)) {
    if (mode == 0) {
      set_lw (q, 0);
      return q;
    } else {
      return n;
    }
  }
// Halfword divide.
  if (HW (n) == 0 && HW (d) == 0) {
    if (mode == 0) {
      set_lw (q, LW (n) / LW (d));
      return q;
    } else {
      set_lw (r, LW (n) % LW (d));
      return r;
    }
  }
// We now know that n and d both have > 64 bits.
// Full divide.
  set_lw (q, 0);
  set_lw (r, 0);
  for (k = 128; k > 0; k--) {
    {
      carry = (LW (q) & D_SIGN) ? 0x1 : 0x0;
      LW (q) <<= 1;
      HW (q) = (HW (q) << 1) | carry;
    }
// Left-shift r
    {
      carry = (LW (r) & D_SIGN) ? 0x1 : 0x0;
      LW (r) <<= 1;
      HW (r) = (HW (r) << 1) | carry;
    }
// r[0] = n[k]
    {
      if (HW (n) & D_SIGN) {
        LW (r) |= 0x1;
      }
      carry = (LW (n) & D_SIGN) ? 0x1 : 0x0;
      LW (n) <<= 1;
      HW (n) = (HW (n) << 1) | carry;
    }
// if r >= d
    if (GE (r, d)) {
// r = r - d
      sub_double (p, m, r, r, d);
// q[k] = 1
      LW (q) |= 0x1;
    }
  }
  if (mode == 0) {
    return q;
  } else {
    return r;
  }
}

DOUBLE_NUM_T double_uadd (NODE_T * p, MOID_T * m, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  (void) p;
  add_double (p, m, w, u, v);
  return w;
}

DOUBLE_NUM_T double_usub (NODE_T * p, MOID_T * m, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  (void) p;
  sub_double (p, m, w, u, v);
  return w;
}

DOUBLE_NUM_T double_umul (NODE_T * p, MOID_T * m, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  m128to128 (p, m, &w, u, v);
  return w;
}

// Signed integer.

DOUBLE_NUM_T double_sadd (NODE_T * p, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  int neg_u = D_NEG (u), neg_v = D_NEG (v);
  set_lw (w, 0);
  if (neg_u) {
    u = neg_double_int (u);
  }
  if (neg_v) {
    v = neg_double_int (v);
  }
  if (!neg_u && !neg_v) {
    w = double_uadd (p, M_LONG_INT, u, v);
    PRELUDE_ERROR (D_NEG (w), p, ERROR_MATH, M_LONG_INT);
  } else if (neg_u && neg_v) {
    w = neg_double_int (double_sadd (p, u, v));
  } else if (neg_u) {
    w = double_ssub (p, v, u);
  } else if (neg_v) {
    w = double_ssub (p, u, v);
  }
  return w;
}

DOUBLE_NUM_T double_ssub (NODE_T * p, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  int neg_u = D_NEG (u), neg_v = D_NEG (v);
  set_lw (w, 0);
  if (neg_u) {
    u = neg_double_int (u);
  }
  if (neg_v) {
    v = neg_double_int (v);
  }
  if (!neg_u && !neg_v) {
    if (D_LT (u, v)) {
      w = neg_double_int (double_usub (p, M_LONG_INT, v, u));
    } else {
      w = double_usub (p, M_LONG_INT, u, v);
    }
  } else if (neg_u && neg_v) {
    w = double_ssub (p, v, u);
  } else if (neg_u) {
    w = neg_double_int (double_sadd (p, u, v));
  } else if (neg_v) {
    w = double_sadd (p, u, v);
  }
  return w;
}

DOUBLE_NUM_T double_smul (NODE_T * p, DOUBLE_NUM_T u, DOUBLE_NUM_T v)
{
  DOUBLE_NUM_T w;
  int neg_u = D_NEG (u), neg_v = D_NEG (v);
  if (neg_u) {
    u = neg_double_int (u);
  }
  if (neg_v) {
    v = neg_double_int (v);
  }
  w = double_umul (p, M_LONG_INT, u, v);
  if (neg_u != neg_v) {
    w = neg_double_int (w);
  }
  return w;
}

DOUBLE_NUM_T double_sdiv (NODE_T * p, DOUBLE_NUM_T u, DOUBLE_NUM_T v, int mode)
{
  DOUBLE_NUM_T w;
  int neg_u = D_NEG (u), neg_v = D_NEG (v);
  if (neg_u) {
    u = neg_double_int (u);
  }
  if (neg_v) {
    v = neg_double_int (v);
  }
  w = double_udiv (p, M_LONG_INT, u, v, mode);
  if (mode == 0 && neg_u != neg_v) {
    w = neg_double_int (w);
  } else if (mode == 1 && D_NEG (w)) {
    w = double_sadd (p, w, v);
  }
  return w;
}

// Infinity.

DOUBLE_T a68_divq (DOUBLE_T x, DOUBLE_T y)
{
  return x / y;
}

DOUBLE_T a68_dposinf (void)
{
  return a68_divq (+1.0, 0.0);
}

DOUBLE_T a68_dneginf (void)
{
  return a68_divq (-1.0, 0.0);
}

//! @brief Sqrt (x^2 + y^2) that does not needlessly overflow.

DOUBLE_T a68_double_hypot (DOUBLE_T x, DOUBLE_T y)
{
  DOUBLE_T xabs = ABSQ (x), yabs = ABSQ (y), min, max;
  if (xabs < yabs) {
    min = xabs;
    max = yabs;
  } else {
    min = yabs;
    max = xabs;
  }
  if (min == 0.0q) {
    return max;
  } else {
    DOUBLE_T u = min / max;
    return max * sqrt_double (1.0q + u * u);
  }
}

// Conversions.

DOUBLE_NUM_T double_int_to_double (NODE_T * p, DOUBLE_NUM_T z)
{
  DOUBLE_NUM_T w, radix;
  DOUBLE_T weight;
  int neg = D_NEG (z);
  if (neg) {
    z = abs_double_int (z);
  }
  w.f = 0.0q;
  set_lw (radix, RADIX);
  weight = 1.0q;
  while (!D_ZERO (z)) {
    DOUBLE_NUM_T digit;
    digit = double_udiv (p, M_LONG_INT, z, radix, 1);
    w.f = w.f + LW (digit) * weight;
    z = double_udiv (p, M_LONG_INT, z, radix, 0);
    weight = weight * RADIX_Q;
  }
  if (neg) {
    w.f = -w.f;
  }
  return w;
}

DOUBLE_NUM_T double_to_double_int (NODE_T * p, DOUBLE_NUM_T z)
{
// This routines looks a lot like "strtol". 
  DOUBLE_NUM_T sum, weight, radix;
  BOOL_T negative = (BOOL_T) (z.f < 0);
  z.f = fabs_double (trunc_double (z.f));
  if (z.f > CONST_2_UP_112_Q) {
    errno = EDOM;
    MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
  }
  set_lw (sum, 0);
  set_lw (weight, 1);
  set_lw (radix, RADIX);
  while (z.f > 0) {
    DOUBLE_NUM_T term, digit, quot, rest;
    quot.f = trunc_double (z.f / RADIX_Q);
    rest.f = z.f - quot.f * RADIX_Q;
    z.f = quot.f;
    set_lw (digit, (INT_T) (rest.f));
    term = double_umul (p, M_LONG_INT, digit, weight);
    sum = double_uadd (p, M_LONG_INT, sum, term);
    if (z.f > 0.0q) {
      weight = double_umul (p, M_LONG_INT, weight, radix);
    }
  }
  if (negative) {
    return neg_double_int (sum);
  } else {
    return sum;
  }
}

//! @brief Value of LONG INT denotation

int string_to_double_int (NODE_T * p, A68_LONG_INT * z, char *s)
{
  int k, end, sign;
  DOUBLE_NUM_T weight, ten, sum;
  while (IS_SPACE (s[0])) {
    s++;
  }
// Get the sign
  sign = (s[0] == '-' ? -1 : 1);
  if (s[0] == '+' || s[0] == '-') {
    s++;
  }
  end = 0;
  while (s[end] != '\0') {
    end++;
  }
  set_lw (sum, 0);
  set_lw (weight, 1);
  set_lw (ten, 10);
  for (k = end - 1; k >= 0; k--) {
    DOUBLE_NUM_T term;
    int digit = s[k] - '0';
    set_lw (term, digit);
    term = double_umul (p, M_LONG_INT, term, weight);
    sum = double_uadd (p, M_LONG_INT, sum, term);
    weight = double_umul (p, M_LONG_INT, weight, ten);
  }
  if (sign == -1) {
    HW (sum) = HW (sum) | D_SIGN;
  }
  VALUE (z) = sum;
  STATUS (z) = INIT_MASK;
  return A68_TRUE;
}

//! @brief LONG BITS value of LONG BITS denotation

DOUBLE_NUM_T double_strtou (NODE_T * p, char *s)
{
  int base = 0;
  DOUBLE_NUM_T z;
  char *radix = NO_TEXT;
  errno = 0;
  base = (int) a68_strtou (s, &radix, 10);
  if (base < 2 || base > 16) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_INVALID_RADIX, base);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  set_lw (z, 0x0);
  if (radix != NO_TEXT && TO_UPPER (radix[0]) == TO_UPPER (RADIX_CHAR) && errno == 0) {
    DOUBLE_NUM_T w;
    char *q = radix;
    while (q[0] != NULL_CHAR) {
      q++;
    }
    set_lw (w, 1);
    while ((--q) != radix) {
      int digit = char_value (q[0]);
      if (digit < 0 && digit >= base) {
        diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_LONG_BITS);
        exit_genie (p, A68_RUNTIME_ERROR);
      } else {
        DOUBLE_NUM_T v;
        set_lw (v, digit);
        v = double_umul (p, M_LONG_INT, v, w);
        z = double_uadd (p, M_LONG_INT, z, v);
        set_lw (v, base);
        w = double_umul (p, M_LONG_INT, w, v);
      }
    }
  } else {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_IN_DENOTATION, M_LONG_BITS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (z);
}

//! @brief OP LENG = (BITS) LONG BITS

void genie_lengthen_bits_to_double_bits (NODE_T * p)
{
  A68_BITS k;
  DOUBLE_NUM_T d;
  POP_OBJECT (p, &k, A68_BITS);
  LW (d) = VALUE (&k);
  HW (d) = 0;
  PUSH_VALUE (p, d, A68_LONG_BITS);
}

//! @brief OP SHORTEN = (LONG BITS) BITS

void genie_shorten_double_bits_to_bits (NODE_T * p)
{
  A68_LONG_BITS k;
  DOUBLE_NUM_T j;
  POP_OBJECT (p, &k, A68_LONG_BITS);
  j = VALUE (&k);
  PRELUDE_ERROR (HW (j) != 0, p, ERROR_MATH, M_BITS);
  PUSH_VALUE (p, LW (j), A68_BITS);
}

//! @brief Convert to other radix, binary up to hexadecimal.

BOOL_T convert_radix_double (NODE_T * p, DOUBLE_NUM_T z, int radix, int width)
{
  DOUBLE_NUM_T w, rad;
  if (radix < 2 || radix > 16) {
    radix = 16;
  }
  set_lw (rad, radix);
  reset_transput_buffer (EDIT_BUFFER);
  if (width > 0) {
    while (width > 0) {
      w = double_udiv (p, M_LONG_INT, z, rad, 1);
      plusto_transput_buffer (p, digchar (LW (w)), EDIT_BUFFER);
      width--;
      z = double_udiv (p, M_LONG_INT, z, rad, 0);
    }
    return D_ZERO (z);
  } else if (width == 0) {
    do {
      w = double_udiv (p, M_LONG_INT, z, rad, 1);
      plusto_transput_buffer (p, digchar (LW (w)), EDIT_BUFFER);
      z = double_udiv (p, M_LONG_INT, z, rad, 0);
    }
    while (!D_ZERO (z));
    return A68_TRUE;
  } else {
    return A68_FALSE;
  }
}

//! @brief OP LENG = (LONG INT) LONG REAL

void genie_widen_double_int_to_double (NODE_T * p)
{
  A68_DOUBLE *z = (A68_DOUBLE *) STACK_TOP;
  EXECUTE_UNIT (SUB (p));
  VALUE (z) = double_int_to_double (p, VALUE (z));
}

//! @brief OP LENG = (REAL) LONG REAL

DOUBLE_NUM_T dble_double (NODE_T * p, REAL_T z)
{
// Quick and dirty, only works with 64-bit INT_T.
  BOOL_T nega = (z < 0.0);
  REAL_T u = fabs (z);
  DOUBLE_NUM_T w;
  int expo = 0;
  standardise (&u, 1, REAL_DIG, &expo);
  u *= ten_up (REAL_DIG);
  expo -= REAL_DIG;
  set_lw (w, (INT_T) u);
  w = double_int_to_double (p, w);
  w.f *= ten_up_double (expo);
  if (nega) {
    w.f = -w.f;
  }
  return w;
}

//! @brief OP LENG = (REAL) LONG REAL

void genie_lengthen_real_to_double (NODE_T * p)
{
  A68_REAL z;
  POP_OBJECT (p, &z, A68_REAL);
  PUSH_VALUE (p, dble_double (p, VALUE (&z)), A68_LONG_REAL);
}

//! @brief OP SHORTEN = (LONG REAL) REAL

void genie_shorten_double_to_real (NODE_T * p)
{
  A68_LONG_REAL z;
  REAL_T w;
  POP_OBJECT (p, &z, A68_LONG_REAL);
  w = VALUE (&z).f;
  PUSH_VALUE (p, w, A68_REAL);
}

//! @brief Convert integer to multi-precison number.

MP_T *double_int_to_mp (NODE_T * p, MP_T * z, DOUBLE_NUM_T k, int digs)
{
  DOUBLE_NUM_T k2, radix;
  int n = 0, j, negative = D_NEG (k);
  if (negative) {
    k = neg_double_int (k);
  }
  set_lw (radix, MP_RADIX);
  k2 = k;
  do {
    k2 = double_udiv (p, M_LONG_INT, k2, radix, 0);
    if (!D_ZERO (k2)) {
      n++;
    }
  }
  while (!D_ZERO (k2));
  SET_MP_ZERO (z, digs);
  MP_EXPONENT (z) = (MP_T) n;
  for (j = 1 + n; j >= 1; j--) {
    DOUBLE_NUM_T term = double_udiv (p, M_LONG_INT, k, radix, 1);
    MP_DIGIT (z, j) = (MP_T) LW (term);
    k = double_udiv (p, M_LONG_INT, k, radix, 0);
  }
  MP_DIGIT (z, 1) = (negative ? -MP_DIGIT (z, 1) : MP_DIGIT (z, 1));
  check_mp_exp (p, z);
  return z;
}

//! @brief Convert multi-precision number to integer.

DOUBLE_NUM_T mp_to_double_int (NODE_T * p, MP_T * z, int digs)
{
// This routines looks a lot like "strtol". 
  int j, expo = (int) MP_EXPONENT (z);
  DOUBLE_NUM_T sum, weight;
  set_lw (sum, 0);
  set_lw (weight, 1);
  BOOL_T negative;
  if (expo >= digs) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, MOID (p));
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  negative = (BOOL_T) (MP_DIGIT (z, 1) < 0);
  if (negative) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  for (j = 1 + expo; j >= 1; j--) {
    DOUBLE_NUM_T term, digit, radix;
    set_lw (digit, (MP_INT_T) MP_DIGIT (z, j));
    term = double_umul (p, M_LONG_INT, digit, weight);
    sum = double_uadd (p, M_LONG_INT, sum, term);
    set_lw (radix, MP_RADIX);
    weight = double_umul (p, M_LONG_INT, weight, radix);
  }
  if (negative) {
    return neg_double_int (sum);
  } else {
    return sum;
  }
}

//! @brief Convert real to multi-precison number.

MP_T *double_to_mp (NODE_T * p, MP_T * z, DOUBLE_T x, int digs)
{
  int j, k, sign_x, sum, weight;
  SET_MP_ZERO (z, digs);
  if (x == 0.0q) {
    return z;
  }
// Small integers can be done better by int_to_mp.
  if (ABS (x) < MP_RADIX && trunc_double (x) == x) {
    return int_to_mp (p, z, (int) trunc_double (x), digs);
  }
  sign_x = SIGN (x);
// Scale to [0, 0.1>.
  DOUBLE_T a = ABS (x);
  INT_T expo = (int) log10_double (a);
  a /= ten_up_double (expo);
  expo--;
  if (a >= 1.0q) {
    a /= 10.0q;
    expo++;
  }
// Transport digits of x to the mantissa of z.
  sum = 0;
  weight = (MP_RADIX / 10);
  for (k = 0, j = 1; a != 0.0q && j <= digs && k < DOUBLE_DIGITS; k++) {
    DOUBLE_T u = a * 10.0q;
    DOUBLE_T v = floor_double (u);
    a = u - v;
    sum += weight * (int) v;
    weight /= 10;
    if (weight < 1) {
      MP_DIGIT (z, j++) = (MP_T) sum;
      sum = 0;
      weight = (MP_RADIX / 10);
    }
  }
// Store the last digits.
  if (j <= digs) {
    MP_DIGIT (z, j) = (MP_T) sum;
  }
  (void) align_mp (z, &expo, digs);
  MP_EXPONENT (z) = (MP_T) expo;
  MP_DIGIT (z, 1) *= sign_x;
  check_mp_exp (p, z);
  return z;
}

//! @brief Convert multi-precision number to real.

DOUBLE_T mp_to_double (NODE_T * p, MP_T * z, int digs)
{
// This routine looks a lot like "strtod".
  if (MP_EXPONENT (z) * (MP_T) LOG_MP_RADIX <= (MP_T) DOUBLE_MIN_10_EXP) {
    return 0;
  } else {
    DOUBLE_T terms[1 + MP_MAX_DIGITS];
    DOUBLE_T weight = ten_up_double ((int) (MP_EXPONENT (z) * LOG_MP_RADIX));
    int lim = MIN (digs, MP_MAX_DIGITS);
    for (unt k = 1; k <= lim; k++) {
      terms[k] = ABS (MP_DIGIT (z, k)) * weight;
      weight /= MP_RADIX;
    }
// Sum terms from small to large.
    DOUBLE_T sum = 0;
    for (unt k = lim; k >= 1; k--) {
      sum += terms[k];
    }
    CHECK_DOUBLE_REAL (p, sum);
    return MP_DIGIT (z, 1) >= 0 ? sum : -sum;
  }
}

DOUBLE_T inverf_double (DOUBLE_T z)
{
  if (fabs_double (z) >= 1.0q) {
    errno = EDOM;
    return z;
  } else {
// Newton-Raphson.
    DOUBLE_T f = sqrt_double (M_PIq) / 2, g, x = z;
    int its = 10;
    x = dble (a68_inverf ((REAL_T) x)).f;
    do {
      g = x;
      x -= f * (erf_double (x) - z) / exp_double (-(x * x));
    } while (its-- > 0 && errno == 0 && fabs_double (x - g) > (3 * FLT128_EPSILON));
    return x;
  }
}

//! @brief OP LENG = (LONG REAL) LONG LONG REAL

void genie_lengthen_double_to_mp (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_REAL);
  A68_LONG_REAL x;
  POP_OBJECT (p, &x, A68_LONG_REAL);
  MP_T *z = nil_mp (p, digs);
  (void) double_to_mp (p, z, VALUE (&x).f, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG LONG REAL) LONG REAL

void genie_shorten_mp_to_double (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *z;
  DOUBLE_NUM_T d;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  d.f = mp_to_double (p, z, digs);
  PUSH_VALUE (p, d, A68_LONG_REAL);
}

//! @brief OP SHORTEN = (LONG LONG COMPLEX) LONG COMPLEX

void genie_shorten_long_mp_complex_to_double_compl (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_REAL), size = SIZE (M_LONG_LONG_REAL);
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  DECREMENT_STACK_POINTER (p, 2 * size);
  DOUBLE_NUM_T u, v;
  u.f = mp_to_double (p, a, digs);
  v.f = mp_to_double (p, b, digs);
  PUSH_VALUE (p, u, A68_LONG_REAL);
  PUSH_VALUE (p, v, A68_LONG_REAL);
}

//! @brief OP LENG = (LONG INT) LONG LONG INT

void genie_lengthen_double_int_to_mp (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_INT);
  A68_LONG_INT k;
  POP_OBJECT (p, &k, A68_LONG_INT);
  MP_T *z = nil_mp (p, digs);
  (void) double_int_to_mp (p, z, VALUE (&k), digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP SHORTEN = (LONG LONG INT) LONG INT

void genie_shorten_mp_to_double_int (NODE_T * p)
{
  MOID_T *mode = LHS_MODE (p);
  int digs = DIGITS (mode), size = SIZE (mode);
  MP_T *z;
  DECREMENT_STACK_POINTER (p, size);
  z = (MP_T *) STACK_TOP;
  MP_STATUS (z) = (MP_T) INIT_MASK;
  PUSH_VALUE (p, mp_to_double_int (p, z, digs), A68_LONG_INT);
}

//! @brief OP LENG = (INT) LONG INT

void genie_lengthen_int_to_double_int (NODE_T * p)
{
  A68_INT k;
  INT_T v;
  DOUBLE_NUM_T d;
  POP_OBJECT (p, &k, A68_INT);
  v = VALUE (&k);
  if (v >= 0) {
    LW (d) = v;
    HW (d) = 0;
  } else {
    LW (d) = -v;
    HW (d) = D_SIGN;
  }
  PUSH_VALUE (p, d, A68_LONG_INT);
}

//! @brief OP SHORTEN = (LONG INT) INT

void genie_shorten_long_int_to_int (NODE_T * p)
{
  A68_LONG_INT k;
  DOUBLE_NUM_T j;
  POP_OBJECT (p, &k, A68_LONG_INT);
  j = VALUE (&k);
  PRELUDE_ERROR (HW (j) != 0 && HW (j) != D_SIGN, p, ERROR_MATH, M_INT);
  PRELUDE_ERROR (LW (j) & D_SIGN, p, ERROR_MATH, M_INT);
  if (D_NEG (j)) {
    PUSH_VALUE (p, -LW (j), A68_INT);
  } else {
    PUSH_VALUE (p, LW (j), A68_INT);
  }
}

// Constants.

//! @brief PROC long max int = LONG INT

void genie_double_max_int (NODE_T * p)
{
  DOUBLE_NUM_T d;
  HW (d) = 0x7fffffffffffffffLL;
  LW (d) = 0xffffffffffffffffLL;
  PUSH_VALUE (p, d, A68_LONG_INT);
}

//! @brief PROC long max bits = LONG BITS

void genie_double_max_bits (NODE_T * p)
{
  DOUBLE_NUM_T d;
  HW (d) = 0xffffffffffffffffLL;
  LW (d) = 0xffffffffffffffffLL;
  PUSH_VALUE (p, d, A68_LONG_INT);
}

//! @brief LONG REAL max long real

void genie_double_max_real (NODE_T * p)
{
  DOUBLE_NUM_T d;
  d.f = FLT128_MAX;
  PUSH_VALUE (p, d, A68_LONG_REAL);
}

//! @brief LONG REAL min long real

void genie_double_min_real (NODE_T * p)
{
  DOUBLE_NUM_T d;
  d.f = FLT128_MIN;
  PUSH_VALUE (p, d, A68_LONG_REAL);
}

//! @brief LONG REAL small long real

void genie_double_small_real (NODE_T * p)
{
  DOUBLE_NUM_T d;
  d.f = FLT128_EPSILON;
  PUSH_VALUE (p, d, A68_LONG_REAL);
}

//! @brief PROC long pi = LON REAL

void genie_pi_double (NODE_T * p)
{
  DOUBLE_NUM_T w;
  w.f = M_PIq;
  PUSH_VALUE (p, w, A68_LONG_INT);
}

// MONADs and DYADs

//! @brief OP SIGN = (LONG INT) INT

void genie_sign_double_int (NODE_T * p)
{
  A68_LONG_INT k;
  POP_OBJECT (p, &k, A68_LONG_INT);
  PUSH_VALUE (p, sign_double_int (VALUE (&k)), A68_INT);
}

//! @brief OP ABS = (LONG INT) LONG INT

void genie_abs_double_int (NODE_T * p)
{
  A68_LONG_INT *k;
  POP_OPERAND_ADDRESS (p, k, A68_LONG_INT);
  VALUE (k) = abs_double_int (VALUE (k));
}

//! @brief OP ODD = (LONG INT) BOOL

void genie_odd_double_int (NODE_T * p)
{
  A68_LONG_INT j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_INT);
  w = abs_double_int (VALUE (&j));
  if (LW (w) & 0x1) {
    PUSH_VALUE (p, A68_TRUE, A68_BOOL);
  } else {
    PUSH_VALUE (p, A68_FALSE, A68_BOOL);
  }
}

//! @brief OP - = (LONG INT) LONG INT

void genie_minus_double_int (NODE_T * p)
{
  A68_LONG_INT *k;
  POP_OPERAND_ADDRESS (p, k, A68_LONG_INT);
  VALUE (k) = neg_double_int (VALUE (k));
}

//! @brief OP ROUND = (LONG REAL) LONG INT

void genie_round_double (NODE_T * p)
{
  A68_LONG_REAL x;
  DOUBLE_NUM_T u;
  POP_OBJECT (p, &x, A68_LONG_REAL);
  u = VALUE (&x);
  if (u.f < 0.0q) {
    u.f = u.f - 0.5q;
  } else {
    u.f = u.f + 0.5q;
  }
  PUSH_VALUE (p, double_to_double_int (p, u), A68_LONG_INT);
}

//! @brief OP ENTIER = (LONG REAL) LONG INT

void genie_entier_double (NODE_T * p)
{
  A68_LONG_REAL x;
  DOUBLE_NUM_T u;
  POP_OBJECT (p, &x, A68_LONG_REAL);
  u = VALUE (&x);
  u.f = floor_double (u.f);
  PUSH_VALUE (p, double_to_double_int (p, u), A68_LONG_INT);
}

//! @brief OP + = (LONG INT, LONG INT) LONG INT

void genie_add_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PUSH_VALUE (p, double_sadd (p, VALUE (&i), VALUE (&j)), A68_LONG_INT);
}

//! @brief OP - = (LONG INT, LONG INT) LONG INT

void genie_sub_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PUSH_VALUE (p, double_ssub (p, VALUE (&i), VALUE (&j)), A68_LONG_INT);
}

//! @brief OP * = (LONG INT, LONG INT) LONG INT

void genie_mul_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PUSH_VALUE (p, double_smul (p, VALUE (&i), VALUE (&j)), A68_LONG_INT);
}

//! @brief OP / = (LONG INT, LONG INT) LONG INT

void genie_over_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PRELUDE_ERROR (D_ZERO (VALUE (&j)), p, ERROR_DIVISION_BY_ZERO, M_LONG_INT);
  PUSH_VALUE (p, double_sdiv (p, VALUE (&i), VALUE (&j), 0), A68_LONG_INT);
}

//! @brief OP MOD = (LONG INT, LONG INT) LONG INT

void genie_mod_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PRELUDE_ERROR (D_ZERO (VALUE (&j)), p, ERROR_DIVISION_BY_ZERO, M_LONG_INT);
  PUSH_VALUE (p, double_sdiv (p, VALUE (&i), VALUE (&j), 1), A68_LONG_INT);
}

//! @brief OP / = (LONG INT, LONG INT) LONG REAL

void genie_div_double_int (NODE_T * p)
{
  A68_LONG_INT i, j;
  DOUBLE_NUM_T u, v, w;
  POP_OBJECT (p, &j, A68_LONG_INT);
  POP_OBJECT (p, &i, A68_LONG_INT);
  PRELUDE_ERROR (D_ZERO (VALUE (&j)), p, ERROR_DIVISION_BY_ZERO, M_LONG_INT);
  v = double_int_to_double (p, VALUE (&j));
  u = double_int_to_double (p, VALUE (&i));
  w.f = u.f / v.f;
  PUSH_VALUE (p, w, A68_LONG_REAL);
}

//! @brief OP ** = (LONG INT, INT) INT

void genie_pow_double_int_int (NODE_T * p)
{
  A68_LONG_INT i;
  A68_INT j;
  UNSIGNED_T expo, top;
  DOUBLE_NUM_T mult, prod;
  POP_OBJECT (p, &j, A68_INT);
  PRELUDE_ERROR (VALUE (&j) < 0, p, ERROR_EXPONENT_INVALID, M_INT);
  top = (UNSIGNED_T) VALUE (&j);
  POP_OBJECT (p, &i, A68_LONG_INT);
  set_lw (prod, 1);
  mult = VALUE (&i);
  expo = 1;
  while (expo <= top) {
    if (expo & top) {
      prod = double_smul (p, prod, mult);
    }
    expo <<= 1;
    if (expo <= top) {
      mult = double_smul (p, mult, mult);
    }
  }
  PUSH_VALUE (p, prod, A68_LONG_INT);
}

//! @brief OP - = (LONG REAL) LONG REAL

void genie_minus_double (NODE_T * p)
{
  A68_LONG_REAL *u;
  POP_OPERAND_ADDRESS (p, u, A68_LONG_REAL);
  VALUE (u).f = -(VALUE (u).f);
}

//! @brief OP ABS = (LONG REAL) LONG REAL

void genie_abs_double (NODE_T * p)
{
  A68_LONG_REAL *u;
  POP_OPERAND_ADDRESS (p, u, A68_LONG_REAL);
  VALUE (u).f = fabs_double (VALUE (u).f);
}

//! @brief OP SIGN = (LONG REAL) INT

void genie_sign_double (NODE_T * p)
{
  A68_LONG_REAL u;
  POP_OBJECT (p, &u, A68_LONG_REAL);
  PUSH_VALUE (p, sign_double (VALUE (&u)), A68_INT);
}

//! @brief OP ** = (LONG REAL, INT) INT

void genie_pow_double_int (NODE_T * p)
{
  A68_LONG_REAL z;
  A68_INT j;
  INT_T top;
  UNSIGNED_T expo;
  DOUBLE_NUM_T mult, prod;
  int negative;
  POP_OBJECT (p, &j, A68_INT);
  top = (UNSIGNED_T) VALUE (&j);
  POP_OBJECT (p, &z, A68_LONG_INT);
  prod.f = 1.0q;
  mult.f = VALUE (&z).f;
  if (top < 0) {
    top = -top;
    negative = A68_TRUE;
  } else {
    negative = A68_FALSE;
  }
  expo = 1;
  while (expo <= top) {
    if (expo & top) {
      prod.f = prod.f * mult.f;
      CHECK_DOUBLE_REAL (p, prod.f);
    }
    expo <<= 1;
    if (expo <= top) {
      mult.f = mult.f * mult.f;
      CHECK_DOUBLE_REAL (p, mult.f);
    }
  }
  if (negative) {
    prod.f = 1.0q / prod.f;
  }
  PUSH_VALUE (p, prod, A68_LONG_REAL);
}

//! @brief OP ** = (LONG REAL, LONG REAL) LONG REAL

void genie_pow_double (NODE_T * p)
{
  A68_LONG_REAL x, y;
  DOUBLE_T z = 0.0q;
  POP_OBJECT (p, &y, A68_LONG_REAL);
  POP_OBJECT (p, &x, A68_LONG_REAL);
  errno = 0;
  PRELUDE_ERROR (VALUE (&x).f < 0.0q, p, ERROR_INVALID_ARGUMENT, M_LONG_REAL);
  if (VALUE (&x).f == 0.0q) {
    if (VALUE (&y).f < 0.0q) {
      errno = ERANGE;
      MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
    } else {
      z = (VALUE (&y).f == 0.0q ? 1.0q : 0.0q);
    }
  } else {
    z = exp_double (VALUE (&y).f * log_double (VALUE (&x).f));
    MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
  }
  PUSH_VALUE (p, dble (z), A68_LONG_REAL);
}

//! @brief OP + = (LONG REAL, LONG REAL) LONG REAL

void genie_add_double (NODE_T * p)
{
  A68_LONG_REAL u, v;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &v, A68_LONG_REAL);
  POP_OBJECT (p, &u, A68_LONG_REAL);
  w.f = VALUE (&u).f + VALUE (&v).f;
  CHECK_DOUBLE_REAL (p, w.f);
  PUSH_VALUE (p, w, A68_LONG_REAL);
}

//! @brief OP - = (LONG REAL, LONG REAL) LONG REAL

void genie_sub_double (NODE_T * p)
{
  A68_LONG_REAL u, v;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &v, A68_LONG_REAL);
  POP_OBJECT (p, &u, A68_LONG_REAL);
  w.f = VALUE (&u).f - VALUE (&v).f;
  CHECK_DOUBLE_REAL (p, w.f);
  PUSH_VALUE (p, w, A68_LONG_REAL);
}

//! @brief OP * = (LONG REAL, LONG REAL) LONG REAL

void genie_mul_double (NODE_T * p)
{
  A68_LONG_REAL u, v;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &v, A68_LONG_REAL);
  POP_OBJECT (p, &u, A68_LONG_REAL);
  w.f = VALUE (&u).f * VALUE (&v).f;
  CHECK_DOUBLE_REAL (p, w.f);
  PUSH_VALUE (p, w, A68_LONG_REAL);
}

//! @brief OP / = (LONG REAL, LONG REAL) LONG REAL

void genie_over_double (NODE_T * p)
{
  A68_LONG_REAL u, v;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &v, A68_LONG_REAL);
  POP_OBJECT (p, &u, A68_LONG_REAL);
  PRELUDE_ERROR (VALUE (&v).f == 0.0q, p, ERROR_DIVISION_BY_ZERO, M_LONG_REAL);
  w.f = VALUE (&u).f / VALUE (&v).f;
  PUSH_VALUE (p, w, A68_LONG_REAL);
}

//! @brief OP +:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_plusab_double_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_INT, genie_add_double_int);
}

//! @brief OP -:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_minusab_double_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_INT, genie_sub_double_int);
}

//! @brief OP *:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_timesab_double_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_INT, genie_mul_double_int);
}

//! @brief OP %:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_overab_double_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_INT, genie_over_double_int);
}

//! @brief OP %*:= = (REF LONG INT, LONG INT) REF LONG INT

void genie_modab_double_int (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_INT, genie_mod_double_int);
}

//! @brief OP +:= = (REF LONG REAL, LONG REAL) REF LONG REAL

void genie_plusab_double (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_REAL, genie_add_double);
}

//! @brief OP -:= = (REF LONG REAL, LONG REAL) REF LONG REAL

void genie_minusab_double (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_REAL, genie_sub_double);
}

//! @brief OP *:= = (REF LONG REAL, LONG REAL) REF LONG REAL

void genie_timesab_double (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_REAL, genie_mul_double);
}

//! @brief OP /:= = (REF LONG REAL, LONG REAL) REF LONG REAL

void genie_divab_double (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_REAL, genie_over_double);
}

// OP (LONG INT, LONG INT) BOOL.

#define A68_CMP_INT(n, OP)\
void n (NODE_T * p) {\
  A68_LONG_INT i, j;\
  int k;\
  POP_OBJECT (p, &j, A68_LONG_INT);\
  POP_OBJECT (p, &i, A68_LONG_INT);\
  k = sign_double_int (double_ssub (p, VALUE (&i), VALUE (&j)));\
  PUSH_VALUE (p, (BOOL_T) (k OP 0), A68_BOOL);\
  }
A68_CMP_INT (genie_eq_double_int, ==)
  A68_CMP_INT (genie_ne_double_int, !=)
  A68_CMP_INT (genie_lt_double_int, <)
  A68_CMP_INT (genie_gt_double_int, >)
  A68_CMP_INT (genie_le_double_int, <=)
  A68_CMP_INT (genie_ge_double_int, >=)
// OP (LONG REAL, LONG REAL) BOOL.
#define A68_CMP_REAL(n, OP)\
void n (NODE_T * p) {\
  A68_LONG_REAL i, j;\
  POP_OBJECT (p, &j, A68_LONG_REAL);\
  POP_OBJECT (p, &i, A68_LONG_REAL);\
  PUSH_VALUE (p, (BOOL_T) (VALUE (&i).f OP VALUE (&j).f), A68_BOOL);\
  }
  A68_CMP_REAL (genie_eq_double, ==)
  A68_CMP_REAL (genie_ne_double, !=)
  A68_CMP_REAL (genie_lt_double, <)
  A68_CMP_REAL (genie_gt_double, >)
  A68_CMP_REAL (genie_le_double, <=)
  A68_CMP_REAL (genie_ge_double, >=)
//! @brief OP NOT = (LONG BITS) LONG BITS
     void genie_not_double_bits (NODE_T * p)
{
  A68_LONG_BITS i;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &i, A68_LONG_BITS);
  HW (w) = ~HW (VALUE (&i));
  LW (w) = ~LW (VALUE (&i));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP = = (LONG BITS, LONG BITS) BOOL.

void genie_eq_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = HW (VALUE (&i)) == HW (VALUE (&j));
  v = LW (VALUE (&i)) == LW (VALUE (&j));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP ~= = (LONG BITS, LONG BITS) BOOL.

void genie_ne_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);    // (i ~= j) == ~ (i = j) 
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = HW (VALUE (&i)) == HW (VALUE (&j));
  v = LW (VALUE (&i)) == LW (VALUE (&j));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_FALSE : A68_TRUE), A68_BOOL);
}

//! @brief OP <= = (LONG BITS, LONG BITS) BOOL

void genie_le_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = (HW (VALUE (&i)) | HW (VALUE (&j))) == HW (VALUE (&j));
  v = (LW (VALUE (&i)) | LW (VALUE (&j))) == LW (VALUE (&j));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP > = (LONG BITS, LONG BITS) BOOL

void genie_gt_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);    // (i > j) == ! (i <= j)
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = (HW (VALUE (&i)) | HW (VALUE (&j))) == HW (VALUE (&j));
  v = (LW (VALUE (&i)) | LW (VALUE (&j))) == LW (VALUE (&j));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_FALSE : A68_TRUE), A68_BOOL);
}

//! @brief OP >= = (LONG BITS, LONG BITS) BOOL

void genie_ge_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);    // (i >= j) == (j <= i)
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = (HW (VALUE (&i)) | HW (VALUE (&j))) == HW (VALUE (&i));
  v = (LW (VALUE (&i)) | LW (VALUE (&j))) == LW (VALUE (&i));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP < = (LONG BITS, LONG BITS) BOOL

void genie_lt_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  BOOL_T u, v;
  POP_OBJECT (p, &j, A68_LONG_BITS);    // (i < j) == ! (i >= j)
  POP_OBJECT (p, &i, A68_LONG_BITS);
  u = (HW (VALUE (&i)) | HW (VALUE (&j))) == HW (VALUE (&i));
  v = (LW (VALUE (&i)) | LW (VALUE (&j))) == LW (VALUE (&i));
  PUSH_VALUE (p, (BOOL_T) (u & v ? A68_FALSE : A68_TRUE), A68_BOOL);
}

//! @brief PROC long bits pack = ([] BOOL) BITS

void genie_double_bits_pack (NODE_T * p)
{
  A68_REF z;
  DOUBLE_NUM_T w;
  A68_ARRAY *arr;
  A68_TUPLE *tup;
  int size;
  POP_REF (p, &z);
  CHECK_REF (p, z, M_ROW_BOOL);
  GET_DESCRIPTOR (arr, tup, &z);
  size = ROW_SIZE (tup);
  PRELUDE_ERROR (size < 0 || size > BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_ROW_BOOL);
  set_lw (w, 0x0);
  if (ROW_SIZE (tup) > 0) {
    UNSIGNED_T bit = 0x0;
    int k, n = 0;
    BYTE_T *base = DEREF (BYTE_T, &ARRAY (arr));
    for (k = UPB (tup); k >= LWB (tup); k--) {
      A68_BOOL *boo = (A68_BOOL *) & (base[INDEX_1_DIM (arr, tup, k)]);
      CHECK_INIT (p, INITIALISED (boo), M_BOOL);
      if (n == 0 || n == BITS_WIDTH) {
        bit = 0x1;
      }
      if (VALUE (boo)) {
        if (n > BITS_WIDTH) {
          LW (w) |= bit;
        } else {
          HW (w) |= bit;
        };
      }
      n++;
      bit <<= 1;
    }
  }
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP AND = (LONG BITS, LONG BITS) LONG BITS

void genie_and_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  HW (w) = HW (VALUE (&i)) & HW (VALUE (&j));
  LW (w) = LW (VALUE (&i)) & LW (VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP OR = (LONG BITS, LONG BITS) LONG BITS

void genie_or_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  HW (w) = HW (VALUE (&i)) | HW (VALUE (&j));
  LW (w) = LW (VALUE (&i)) | LW (VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP XOR = (LONG BITS, LONG BITS) LONG BITS

void genie_xor_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  HW (w) = HW (VALUE (&i)) ^ HW (VALUE (&j));
  LW (w) = LW (VALUE (&i)) ^ LW (VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP + = (LONG BITS, LONG BITS) LONG BITS

void genie_add_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  add_double (p, M_LONG_BITS, w, VALUE (&i), VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP - = (LONG BITS, LONG BITS) LONG BITS

void genie_sub_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  sub_double (p, M_LONG_BITS, w, VALUE (&i), VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP * = (LONG BITS, LONG BITS) LONG BITS

void genie_times_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  w = double_umul (p, M_LONG_BITS, VALUE (&i), VALUE (&j));
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP OVER = (LONG BITS, LONG BITS) LONG BITS

void genie_over_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  w = double_udiv (p, M_LONG_BITS, VALUE (&i), VALUE (&j), 0);
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP MOD = (LONG BITS, LONG BITS) LONG BITS

void genie_mod_double_bits (NODE_T * p)
{
  A68_LONG_BITS i, j;
  DOUBLE_NUM_T w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  w = double_udiv (p, M_LONG_BITS, VALUE (&i), VALUE (&j), 1);
  PUSH_VALUE (p, w, A68_LONG_BITS);
}

//! @brief OP ELEM = (INT, LONG BITS) BOOL

void genie_elem_double_bits (NODE_T * p)
{
  A68_LONG_BITS j;
  A68_INT i;
  int k, n;
  UNSIGNED_T mask = 0x1, *w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_INT);
  k = VALUE (&i);
  PRELUDE_ERROR (k < 1 || k > LONG_BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  if (k <= BITS_WIDTH) {
    w = &(LW (VALUE (&j)));
  } else {
    w = &(HW (VALUE (&j)));
    k -= BITS_WIDTH;
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  PUSH_VALUE (p, (BOOL_T) ((*w & mask) ? A68_TRUE : A68_FALSE), A68_BOOL);
}

//! @brief OP SET = (INT, LONG BITS) LONG BITS

void genie_set_double_bits (NODE_T * p)
{
  A68_LONG_BITS j;
  A68_INT i;
  int k, n;
  UNSIGNED_T mask = 0x1, *w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_INT);
  k = VALUE (&i);
  PRELUDE_ERROR (k < 1 || k > LONG_BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  if (k <= BITS_WIDTH) {
    w = &(LW (VALUE (&j)));
  } else {
    w = &(HW (VALUE (&j)));
    k -= BITS_WIDTH;
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  (*w) |= mask;
  PUSH_OBJECT (p, j, A68_LONG_BITS);
}

//! @brief OP CLEAR = (INT, LONG BITS) LONG BITS

void genie_clear_double_bits (NODE_T * p)
{
  A68_LONG_BITS j;
  A68_INT i;
  int k, n;
  UNSIGNED_T mask = 0x1, *w;
  POP_OBJECT (p, &j, A68_LONG_BITS);
  POP_OBJECT (p, &i, A68_INT);
  k = VALUE (&i);
  PRELUDE_ERROR (k < 1 || k > LONG_BITS_WIDTH, p, ERROR_OUT_OF_BOUNDS, M_INT);
  if (k <= BITS_WIDTH) {
    w = &(LW (VALUE (&j)));
  } else {
    w = &(HW (VALUE (&j)));
    k -= BITS_WIDTH;
  }
  for (n = 0; n < (BITS_WIDTH - VALUE (&i)); n++) {
    mask = mask << 1;
  }
  (*w) &= ~mask;
  PUSH_OBJECT (p, j, A68_LONG_BITS);
}

//! @brief OP SHL = (LONG BITS, INT) LONG BITS

void genie_shl_double_bits (NODE_T * p)
{
  A68_LONG_BITS i;
  A68_INT j;
  DOUBLE_NUM_T *w;
  int k, n;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  w = &VALUE (&i);
  k = VALUE (&j);
  if (VALUE (&j) >= 0) {
    for (n = 0; n < k; n++) {
      UNSIGNED_T carry = ((LW (*w) & D_SIGN) ? 0x1 : 0x0);
      PRELUDE_ERROR (MODCHK (p, M_LONG_BITS, HW (*w) | D_SIGN), p, ERROR_MATH, M_LONG_BITS);
      HW (*w) = (HW (*w) << 1) | carry;
      LW (*w) = (LW (*w) << 1);
    }
  } else {
    k = -k;
    for (n = 0; n < k; n++) {
      UNSIGNED_T carry = ((HW (*w) & 0x1) ? D_SIGN : 0x0);
      HW (*w) = (HW (*w) >> 1);
      LW (*w) = (LW (*w) >> 1) | carry;
    }
  }
  PUSH_OBJECT (p, i, A68_LONG_BITS);
}

//! @brief OP SHR = (LONG BITS, INT) LONG BITS

void genie_shr_double_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_shl_double_bits (p);    // Conform RR
}

//! @brief OP ROL = (LONG BITS, INT) LONG BITS

void genie_rol_double_bits (NODE_T * p)
{
  A68_LONG_BITS i;
  A68_INT j;
  DOUBLE_NUM_T *w = &VALUE (&i);
  int k, n;
  POP_OBJECT (p, &j, A68_INT);
  POP_OBJECT (p, &i, A68_LONG_BITS);
  k = VALUE (&j);
  if (k >= 0) {
    for (n = 0; n < k; n++) {
      UNSIGNED_T carry = ((HW (*w) & D_SIGN) ? 0x1 : 0x0);
      UNSIGNED_T carry_between = ((LW (*w) & D_SIGN) ? 0x1 : 0x0);
      HW (*w) = (HW (*w) << 1) | carry_between;
      LW (*w) = (LW (*w) << 1) | carry;
    }
  } else {
    k = -k;
    for (n = 0; n < k; n++) {
      UNSIGNED_T carry = ((LW (*w) & 0x1) ? D_SIGN : 0x0);
      UNSIGNED_T carry_between = ((HW (*w) & 0x1) ? D_SIGN : 0x0);
      HW (*w) = (HW (*w) >> 1) | carry;
      LW (*w) = (LW (*w) >> 1) | carry_between;
    }
  }
  PUSH_OBJECT (p, i, A68_LONG_BITS);
}

//! @brief OP ROR = (LONG BITS, INT) LONG BITS

void genie_ror_double_bits (NODE_T * p)
{
  A68_INT *j;
  POP_OPERAND_ADDRESS (p, j, A68_INT);
  VALUE (j) = -VALUE (j);
  genie_rol_double_bits (p);    // Conform RR
}

//! @brief OP BIN = (LONG INT) LONG BITS

void genie_bin_double_int (NODE_T * p)
{
  A68_LONG_INT i;
  POP_OBJECT (p, &i, A68_LONG_INT);
// RR does not convert negative numbers
  if (D_NEG (VALUE (&i))) {
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_OUT_OF_BOUNDS, M_BITS);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  PUSH_OBJECT (p, i, A68_LONG_BITS);
}

//! @brief OP +* = (LONG REAL, LONG REAL) LONG COMPLEX

void genie_i_double_compl (NODE_T * p)
{
  (void) p;
}

//! @brief OP SHORTEN = (LONG COMPLEX) COMPLEX

void genie_shorten_double_compl_to_complex (NODE_T * p)
{
  A68_LONG_REAL re, im;
  REAL_T w;
  POP_OBJECT (p, &im, A68_LONG_REAL);
  POP_OBJECT (p, &re, A68_LONG_REAL);
  w = VALUE (&re).f;
  PUSH_VALUE (p, w, A68_REAL);
  w = VALUE (&im).f;
  PUSH_VALUE (p, w, A68_REAL);
}

//! @brief OP LENG = (LONG COMPLEX) LONG LONG COMPLEX

void genie_lengthen_double_compl_to_long_mp_complex (NODE_T * p)
{
  int digs = DIGITS (M_LONG_LONG_REAL);
  A68_LONG_REAL re, im;
  POP_OBJECT (p, &im, A68_LONG_REAL);
  POP_OBJECT (p, &re, A68_LONG_REAL);
  MP_T *z = nil_mp (p, digs);
  (void) double_to_mp (p, z, VALUE (&re).f, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
  z = nil_mp (p, digs);
  (void) double_to_mp (p, z, VALUE (&im).f, digs);
  MP_STATUS (z) = (MP_T) INIT_MASK;
}

//! @brief OP +* = (LONG INT, LONG INT) LONG COMPLEX

void genie_i_int_double_compl (NODE_T * p)
{
  A68_LONG_INT re, im;
  POP_OBJECT (p, &im, A68_LONG_INT);
  POP_OBJECT (p, &re, A68_LONG_INT);
  PUSH_VALUE (p, double_int_to_double (p, VALUE (&re)), A68_LONG_REAL);
  PUSH_VALUE (p, double_int_to_double (p, VALUE (&im)), A68_LONG_REAL);
}

//! @brief OP RE = (LONG COMPLEX) LONG REAL

void genie_re_double_compl (NODE_T * p)
{
  DECREMENT_STACK_POINTER (p, SIZE (M_LONG_REAL));
}

//! @brief OP IM = (LONG COMPLEX) LONG REAL

void genie_im_double_compl (NODE_T * p)
{
  A68_LONG_REAL re, im;
  POP_OBJECT (p, &im, A68_LONG_REAL);
  POP_OBJECT (p, &re, A68_LONG_REAL);
  PUSH_OBJECT (p, im, A68_LONG_REAL);
}

//! @brief OP - = (LONG COMPLEX) LONG COMPLEX

void genie_minus_double_compl (NODE_T * p)
{
  A68_LONG_REAL re, im;
  POP_OBJECT (p, &im, A68_LONG_REAL);
  POP_OBJECT (p, &re, A68_LONG_REAL);
  VALUE (&re).f = -VALUE (&re).f;
  VALUE (&im).f = -VALUE (&im).f;
  PUSH_OBJECT (p, im, A68_LONG_REAL);
  PUSH_OBJECT (p, re, A68_LONG_REAL);
}

//! @brief OP ABS = (LONG COMPLEX) LONG REAL

void genie_abs_double_compl (NODE_T * p)
{
  A68_LONG_REAL re, im;
  POP_LONG_COMPLEX (p, &re, &im);
  PUSH_VALUE (p, dble (a68_double_hypot (VALUE (&re).f, VALUE (&im).f)), A68_LONG_REAL);
}

//! @brief OP ARG = (LONG COMPLEX) LONG REAL

void genie_arg_double_compl (NODE_T * p)
{
  A68_LONG_REAL re, im;
  POP_LONG_COMPLEX (p, &re, &im);
  PRELUDE_ERROR (VALUE (&re).f == 0.0q && VALUE (&im).f == 0.0q, p, ERROR_INVALID_ARGUMENT, M_LONG_COMPLEX);
  PUSH_VALUE (p, dble (atan2_double (VALUE (&im).f, VALUE (&re).f)), A68_LONG_REAL);
}

//! @brief OP CONJ = (LONG COMPLEX) LONG COMPLEX

void genie_conj_double_compl (NODE_T * p)
{
  A68_LONG_REAL im;
  POP_OBJECT (p, &im, A68_LONG_REAL);
  VALUE (&im).f = -VALUE (&im).f;
  PUSH_OBJECT (p, im, A68_LONG_REAL);
}

//! @brief OP + = (COMPLEX, COMPLEX) COMPLEX

void genie_add_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  VALUE (&re_x).f += VALUE (&re_y).f;
  VALUE (&im_x).f += VALUE (&im_y).f;
  CHECK_DOUBLE_COMPLEX (p, VALUE (&im_x).f, VALUE (&im_y).f);
  PUSH_OBJECT (p, re_x, A68_LONG_REAL);
  PUSH_OBJECT (p, im_x, A68_LONG_REAL);
}

//! @brief OP - = (COMPLEX, COMPLEX) COMPLEX

void genie_sub_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  VALUE (&re_x).f -= VALUE (&re_y).f;
  VALUE (&im_x).f -= VALUE (&im_y).f;
  CHECK_DOUBLE_COMPLEX (p, VALUE (&im_x).f, VALUE (&im_y).f);
  PUSH_OBJECT (p, re_x, A68_LONG_REAL);
  PUSH_OBJECT (p, im_x, A68_LONG_REAL);
}

//! @brief OP * = (COMPLEX, COMPLEX) COMPLEX

void genie_mul_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  DOUBLE_T re, im;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  re = VALUE (&re_x).f * VALUE (&re_y).f - VALUE (&im_x).f * VALUE (&im_y).f;
  im = VALUE (&im_x).f * VALUE (&re_y).f + VALUE (&re_x).f * VALUE (&im_y).f;
  CHECK_DOUBLE_COMPLEX (p, VALUE (&im_x).f, VALUE (&im_y).f);
  PUSH_VALUE (p, dble (re), A68_LONG_REAL);
  PUSH_VALUE (p, dble (im), A68_LONG_REAL);
}

//! @brief OP / = (COMPLEX, COMPLEX) COMPLEX

void genie_div_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  DOUBLE_T re = 0.0, im = 0.0;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  PRELUDE_ERROR (VALUE (&re_y).f == 0.0q && VALUE (&im_y).f == 0.0q, p, ERROR_DIVISION_BY_ZERO, M_LONG_COMPLEX);
  if (ABSQ (VALUE (&re_y).f) >= ABSQ (VALUE (&im_y).f)) {
    DOUBLE_T r = VALUE (&im_y).f / VALUE (&re_y).f, den = VALUE (&re_y).f + r * VALUE (&im_y).f;
    re = (VALUE (&re_x).f + r * VALUE (&im_x).f) / den;
    im = (VALUE (&im_x).f - r * VALUE (&re_x).f) / den;
  } else {
    DOUBLE_T r = VALUE (&re_y).f / VALUE (&im_y).f, den = VALUE (&im_y).f + r * VALUE (&re_y).f;
    re = (VALUE (&re_x).f * r + VALUE (&im_x).f) / den;
    im = (VALUE (&im_x).f * r - VALUE (&re_x).f) / den;
  }
  PUSH_VALUE (p, dble (re), A68_LONG_REAL);
  PUSH_VALUE (p, dble (im), A68_LONG_REAL);
}

//! @brief OP ** = (LONG COMPLEX, INT) LONG COMPLEX

void genie_pow_double_compl_int (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x;
  DOUBLE_T re_y, im_y, re_z, im_z;
  A68_INT j;
  INT_T expo;
  BOOL_T negative;
  POP_OBJECT (p, &j, A68_INT);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  re_z = 1.0q;
  im_z = 0.0q;
  re_y = VALUE (&re_x).f;
  im_y = VALUE (&im_x).f;
  expo = 1;
  negative = (BOOL_T) (VALUE (&j) < 0);
  if (negative) {
    VALUE (&j) = -VALUE (&j);
  }
  while ((UNSIGNED_T) expo <= (UNSIGNED_T) (VALUE (&j))) {
    DOUBLE_T z;
    if (expo & VALUE (&j)) {
      z = re_z * re_y - im_z * im_y;
      im_z = re_z * im_y + im_z * re_y;
      re_z = z;
    }
    z = re_y * re_y - im_y * im_y;
    im_y = im_y * re_y + re_y * im_y;
    re_y = z;
    CHECK_DOUBLE_COMPLEX (p, re_y, im_y);
    CHECK_DOUBLE_COMPLEX (p, re_z, im_z);
    expo <<= 1;
  }
  if (negative) {
    PUSH_VALUE (p, dble (1.0q), A68_LONG_REAL);
    PUSH_VALUE (p, dble (0.0q), A68_LONG_REAL);
    PUSH_VALUE (p, dble (re_z), A68_LONG_REAL);
    PUSH_VALUE (p, dble (im_z), A68_LONG_REAL);
    genie_div_double_compl (p);
  } else {
    PUSH_VALUE (p, dble (re_z), A68_LONG_REAL);
    PUSH_VALUE (p, dble (im_z), A68_LONG_REAL);
  }
}

//! @brief OP = = (COMPLEX, COMPLEX) BOOL

void genie_eq_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  PUSH_VALUE (p, (BOOL_T) ((VALUE (&re_x).f == VALUE (&re_y).f) && (VALUE (&im_x).f == VALUE (&im_y).f)), A68_BOOL);
}

//! @brief OP /= = (COMPLEX, COMPLEX) BOOL

void genie_ne_double_compl (NODE_T * p)
{
  A68_LONG_REAL re_x, im_x, re_y, im_y;
  POP_LONG_COMPLEX (p, &re_y, &im_y);
  POP_LONG_COMPLEX (p, &re_x, &im_x);
  PUSH_VALUE (p, (BOOL_T) ! ((VALUE (&re_x).f == VALUE (&re_y).f) && (VALUE (&im_x).f == VALUE (&im_y).f)), A68_BOOL);
}

//! @brief OP +:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_plusab_double_compl (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_COMPLEX, genie_add_double_compl);
}

//! @brief OP -:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_minusab_double_compl (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_COMPLEX, genie_sub_double_compl);
}

//! @brief OP *:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_timesab_double_compl (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_COMPLEX, genie_mul_double_compl);
}

//! @brief OP /:= = (REF COMPLEX, COMPLEX) REF COMPLEX

void genie_divab_double_compl (NODE_T * p)
{
  genie_f_and_becomes (p, M_REF_LONG_COMPLEX, genie_div_double_compl);
}

//! @brief OP LENG = (COMPLEX) LONG COMPLEX 

void genie_lengthen_complex_to_double_compl (NODE_T * p)
{
  A68_REAL i;
  POP_OBJECT (p, &i, A68_REAL);
  genie_lengthen_real_to_double (p);
  PUSH_OBJECT (p, i, A68_REAL);
  genie_lengthen_real_to_double (p);
}

// Functions

#define CD_FUNCTION(name, fun)\
void name (NODE_T * p) {\
  A68_LONG_REAL *x;\
  POP_OPERAND_ADDRESS (p, x, A68_LONG_REAL);\
  errno = 0;\
  VALUE (x).f = fun (VALUE (x).f);\
  MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);\
}

CD_FUNCTION (genie_acos_double, acos_double);
CD_FUNCTION (genie_acosh_double, acosh_double);
CD_FUNCTION (genie_asinh_double, asinh_double);
CD_FUNCTION (genie_atanh_double, atanh_double);
CD_FUNCTION (genie_asin_double, asin_double);
CD_FUNCTION (genie_atan_double, atan_double);
CD_FUNCTION (genie_cosh_double, cosh_double);
CD_FUNCTION (genie_cos_double, cos_double);
CD_FUNCTION (genie_curt_double, cbrt_double);
CD_FUNCTION (genie_exp_double, exp_double);
CD_FUNCTION (genie_ln_double, log_double);
CD_FUNCTION (genie_log_double, log10_double);
CD_FUNCTION (genie_sinh_double, sinh_double);
CD_FUNCTION (genie_sin_double, sin_double);
CD_FUNCTION (genie_sqrt_double, sqrt_double);
CD_FUNCTION (genie_tanh_double, tanh_double);
CD_FUNCTION (genie_tan_double, tan_double);
CD_FUNCTION (genie_erf_double, erf_double);
CD_FUNCTION (genie_erfc_double, erfc_double);
CD_FUNCTION (genie_lngamma_double, lgamma_double);
CD_FUNCTION (genie_gamma_double, tgamma_double);
CD_FUNCTION (genie_csc_double, csc_double);
CD_FUNCTION (genie_acsc_double, acsc_double);
CD_FUNCTION (genie_sec_double, sec_double);
CD_FUNCTION (genie_asec_double, asec_double);
CD_FUNCTION (genie_cot_double, cot_double);
CD_FUNCTION (genie_acot_double, acot_double);
CD_FUNCTION (genie_sindg_double, sindg_double);
CD_FUNCTION (genie_cosdg_double, cosdg_double);
CD_FUNCTION (genie_tandg_double, tandg_double);
CD_FUNCTION (genie_asindg_double, asindg_double);
CD_FUNCTION (genie_acosdg_double, acosdg_double);
CD_FUNCTION (genie_atandg_double, atandg_double);
CD_FUNCTION (genie_cotdg_double, cotdg_double);
CD_FUNCTION (genie_acotdg_double, acotdg_double);
CD_FUNCTION (genie_sinpi_double, sinpi_double);
CD_FUNCTION (genie_cospi_double, cospi_double);
CD_FUNCTION (genie_tanpi_double, tanpi_double);
CD_FUNCTION (genie_cotpi_double, cotpi_double);

//! @brief PROC long arctan2 = (LONG REAL) LONG REAL

void genie_atan2_double (NODE_T * p)
{
  A68_LONG_REAL x, y;
  POP_OBJECT (p, &y, A68_LONG_REAL);
  POP_OBJECT (p, &x, A68_LONG_REAL);
  errno = 0;
  PRELUDE_ERROR (VALUE (&x).f == 0.0q && VALUE (&y).f == 0.0q, p, ERROR_INVALID_ARGUMENT, M_LONG_REAL);
  VALUE (&x).f = a68_atan2 (VALUE (&y).f, VALUE (&x).f);
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
  PUSH_OBJECT (p, x, A68_LONG_REAL);
}

//! @brief PROC long arctan2dg = (LONG REAL) LONG REAL

void genie_atan2dg_double (NODE_T * p)
{
  A68_LONG_REAL x, y;
  POP_OBJECT (p, &y, A68_LONG_REAL);
  POP_OBJECT (p, &x, A68_LONG_REAL);
  errno = 0;
  PRELUDE_ERROR (VALUE (&x).f == 0.0q && VALUE (&y).f == 0.0q, p, ERROR_INVALID_ARGUMENT, M_LONG_REAL);
  VALUE (&x).f = CONST_180_OVER_PI_Q * a68_atan2 (VALUE (&y).f, VALUE (&x).f);
  PRELUDE_ERROR (errno != 0, p, ERROR_MATH_EXCEPTION, NO_TEXT);
  PUSH_OBJECT (p, x, A68_LONG_REAL);
}

//! @brief PROC (LONG REAL) LONG REAL inverf

void genie_inverf_double (NODE_T * _p_)
{
  A68_LONG_REAL x;
  DOUBLE_T y, z;
  POP_OBJECT (_p_, &x, A68_LONG_REAL);
  errno = 0;
  y = VALUE (&x).f;
  z = inverf_double (y);
  MATH_RTE (_p_, errno != 0, M_LONG_REAL, NO_TEXT);
  CHECK_DOUBLE_REAL (_p_, z);
  PUSH_VALUE (_p_, dble (z), A68_LONG_REAL);
}

//! @brief PROC (LONG REAL) LONG REAL inverfc

void genie_inverfc_double (NODE_T * p)
{
  A68_LONG_REAL *u;
  POP_OPERAND_ADDRESS (p, u, A68_LONG_REAL);
  VALUE (u).f = 1.0q - (VALUE (u).f);
  genie_inverf_double (p);
}

#define CD_C_FUNCTION(p, g)\
  A68_LONG_REAL re, im;\
  DOUBLE_COMPLEX_T z;\
  POP_OBJECT (p, &im, A68_LONG_REAL);\
  POP_OBJECT (p, &re, A68_LONG_REAL);\
  errno = 0;\
  z = VALUE (&re).f + VALUE (&im).f * _Complex_I;\
  z = g (z);\
  PUSH_VALUE (p, dble ((DOUBLE_T) creal_double (z)), A68_LONG_REAL);\
  PUSH_VALUE (p, dble ((DOUBLE_T) cimag_double (z)), A68_LONG_REAL);\
  MATH_RTE (p, errno != 0, M_COMPLEX, NO_TEXT);

//! @brief PROC long csqrt = (LONG COMPLEX) LONG COMPLEX

void genie_sqrt_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, csqrt_double);
}

//! @brief PROC long csin = (LONG COMPLEX) LONG COMPLEX

void genie_sin_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, csin_double);
}

//! @brief PROC long ccos = (LONG COMPLEX) LONG COMPLEX

void genie_cos_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, ccos_double);
}

//! @brief PROC long ctan = (LONG COMPLEX) LONG COMPLEX

void genie_tan_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, ctan_double);
}

//! @brief PROC long casin = (LONG COMPLEX) LONG COMPLEX

void genie_asin_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, casin_double);
}

//! @brief PROC long cacos = (LONG COMPLEX) LONG COMPLEX

void genie_acos_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, cacos_double);
}

//! @brief PROC long catan = (LONG COMPLEX) LONG COMPLEX

void genie_atan_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, catan_double);
}

//! @brief PROC long cexp = (LONG COMPLEX) LONG COMPLEX

void genie_exp_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, cexp_double);
}

//! @brief PROC long cln = (LONG COMPLEX) LONG COMPLEX

void genie_ln_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, clog_double);
}

//! @brief PROC long csinh = (LONG COMPLEX) LONG COMPLEX

void genie_sinh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, csinh_double);
}

//! @brief PROC long ccosh = (LONG COMPLEX) LONG COMPLEX

void genie_cosh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, ccosh_double);
}

//! @brief PROC long ctanh = (LONG COMPLEX) LONG COMPLEX

void genie_tanh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, ctanh_double);
}

//! @brief PROC long casinh = (LONG COMPLEX) LONG COMPLEX

void genie_asinh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, casinh_double);
}

//! @brief PROC long cacosh = (LONG COMPLEX) LONG COMPLEX

void genie_acosh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, cacosh_double);
}

//! @brief PROC long catanh = (LONG COMPLEX) LONG COMPLEX

void genie_atanh_double_compl (NODE_T * p)
{
  CD_C_FUNCTION (p, catanh_double);
}

//! @brief PROC next long random = LONG REAL

void genie_next_random_double (NODE_T * p)
{
// This is 'real width' digits only.
  genie_next_random (p);
  genie_lengthen_real_to_double (p);
}

#define CALL(g, x, y) {\
  ADDR_T pop_sp = A68_SP;\
  A68_LONG_REAL *z = (A68_LONG_REAL *) STACK_TOP;\
  DOUBLE_NUM_T _w_;\
  _w_.f = (x);\
  PUSH_VALUE (_p_, _w_, A68_LONG_REAL);\
  genie_call_procedure (_p_, M_PROC_LONG_REAL_LONG_REAL, M_PROC_LONG_REAL_LONG_REAL, M_PROC_LONG_REAL_LONG_REAL, &(g), pop_sp, pop_fp);\
  (y) = VALUE (z).f;\
  A68_SP = pop_sp;\
}

//! @brief Transform string into real-16.

DOUBLE_T string_to_double (char *s, char **end)
{
  int i, dot = -1, pos = 0, pow = 0, expo;
  DOUBLE_T sum, W, y[FLT128_DIG];
  errno = 0;
  for (i = 0; i < FLT128_DIG; i++) {
    y[i] = 0.0q;
  }
  while (IS_SPACE (s[0])) {
    s++;
  }
// Scan mantissa digits and put them into "y".
  if (s[0] == '-') {
    W = -1.0q;
  } else {
    W = 1.0q;
  }
  if (s[0] == '+' || s[0] == '-') {
    s++;
  }
  while (s[0] == '0') {
    s++;
  }
  while (pow < FLT128_DIG && s[pos] != NULL_CHAR && (IS_DIGIT (s[pos]) || s[pos] == POINT_CHAR)) {
    if (s[pos] == POINT_CHAR) {
      dot = pos;
    } else {
      int val = (int) s[pos] - (int) '0';
      y[pow] = W * val;
      W /= 10.0q;
      pow++;
    }
    pos++;
  }
  (*end) = &(s[pos]);
// Sum from low to high to preserve precision.
  sum = 0.0q;
  for (i = FLT128_DIG - 1; i >= 0; i--) {
    sum = sum + y[i];
  }
// See if there is an exponent.
  if (s[pos] != NULL_CHAR && TO_UPPER (s[pos]) == TO_UPPER (EXPONENT_CHAR)) {
    expo = (int) strtol (&(s[++pos]), end, 10);
  } else {
    expo = 0;
  }
// Standardise.
  if (dot >= 0) {
    expo += dot - 1;
  } else {
    expo += pow - 1;
  }
  while (sum != 0.0q && fabs_double (sum) < 1.0q) {
    sum *= 10.0q;
    expo -= 1;
  }
//
  if (errno == 0) {
    return sum * ten_up_double (expo);
  } else {
    return 0.0q;
  }
}

void genie_beta_inc_cf_double (NODE_T * p)
{
  A68_LONG_REAL x, s, t;
  POP_OBJECT (p, &x, A68_LONG_REAL);
  POP_OBJECT (p, &t, A68_LONG_REAL);
  POP_OBJECT (p, &s, A68_LONG_REAL);
  errno = 0;
  PUSH_VALUE (p, dble (a68_beta_inc_double (VALUE (&s).f, VALUE (&t).f, VALUE (&x).f)), A68_LONG_REAL);
  MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
}

void genie_beta_double (NODE_T * p)
{
  A68_LONG_REAL a, b;
  POP_OBJECT (p, &b, A68_LONG_REAL);
  POP_OBJECT (p, &a, A68_LONG_REAL);
  errno = 0;
  PUSH_VALUE (p, dble (exp_double (lgamma_double (VALUE (&a).f) + lgamma_double (VALUE (&b).f) - lgamma_double (VALUE (&a).f + VALUE (&b).f))), A68_LONG_REAL);
  MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
}

void genie_ln_beta_double (NODE_T * p)
{
  A68_LONG_REAL a, b;
  POP_OBJECT (p, &b, A68_LONG_REAL);
  POP_OBJECT (p, &a, A68_LONG_REAL);
  errno = 0;
  PUSH_VALUE (p, dble (lgamma_double (VALUE (&a).f) + lgamma_double (VALUE (&b).f) - lgamma_double (VALUE (&a).f + VALUE (&b).f)), A68_LONG_REAL);
  MATH_RTE (p, errno != 0, M_LONG_REAL, NO_TEXT);
}

// LONG REAL infinity

void genie_infinity_double (NODE_T * p)
{
  PUSH_VALUE (p, dble (a68_posinf ()), A68_LONG_REAL);
}

// LONG REAL minus infinity

void genie_minus_infinity_double (NODE_T * p)
{
  PUSH_VALUE (p, dble (a68_dneginf ()), A68_LONG_REAL);
}

#endif
