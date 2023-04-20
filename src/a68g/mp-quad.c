//! @file mp-quad.c
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
//! Fixed-length LONG LONG REAL and COMPLEX routines.

#include "a68g.h"

#if (A68_LEVEL >= 3)

#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-double.h"
#include "a68g-quad.h"

//! @brief Convert mp to quad real.

QUAD_T mp_to_quad_real (NODE_T * p, MP_T * z, int digits)
{
// This routine looks a lot like "strtod".
  (void) p;
  QUAD_T u = QUAD_REAL_ZERO;
  if (MP_EXPONENT (z) * (MP_T) LOG_MP_RADIX > (MP_T) REAL_MIN_10_EXP) {
    BOOL_T neg = MP_DIGIT (z, 1) < 0;
    MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
    int expo = (int) (MP_EXPONENT (z) * LOG_MP_RADIX);
    QUAD_T w = ten_up_quad_real (expo), r = double_real_to_quad_real (MP_RADIX);
    for (int j = 1; j <= digits; j++) {
      QUAD_T term = double_real_to_quad_real (MP_DIGIT (z, j));
      term = mul_quad_real (term, w);
      u = _add_quad_real_ (u, term);
      w = div_quad_real (w, r);
    }
    if (neg) {
      u = neg_quad_real (u);
    }
  }
  return u;
}

//! @brief Convert quad real to mp number.

void quad_real_to_mp (NODE_T * p, MP_T *z, QUAD_T x, int digits)
{
  SET_MP_ZERO (z, digits);
  if (is0_quad_real (&x)) {
    return;
  }
  int sign_x = getsgn_quad_real (&x);
// Scale to [0, 0.1>.
// a = ABS (x);
  QUAD_T u = abs_quad_real (x);
// expo = (int) log10q (a);
  QUAD_T v = log10_quad_real (u);
  INT_T expo = (INT_T) quad_real_to_double_real (v);
// v /= ten_up_double (expo);
  v = ten_up_quad_real (expo);
  u = div_quad_real (u, v);
  expo--;
  if (real_cmp_quad_real (&u, &QUAD_REAL_ONE) >= 0) {
    u = div_quad_real (u, QUAD_REAL_TEN);
    expo++;
  }
// Transport digits of x to the mantissa of z.
  INT_T sum = 0, W = (MP_RADIX / 10); int j, k;
  for (k = 0, j = 1; j <= digits && k < QUAD_DIGITS; k++) {
    QUAD_T t = mul_quad_real (u, QUAD_REAL_TEN);
    v = floor_quad_real (t);
    u = frac_quad_real (t);
    sum += (INT_T) (W * quad_real_to_double_real (v));
    W /= 10;
    if (W < 1) {
      MP_DIGIT (z, j++) = (MP_T) sum;
      sum = 0;
      W = (MP_RADIX / 10);
    }
  }
// Store the last digits.
  if (j <= digits) {
    MP_DIGIT (z, j) = (MP_T) sum;
  }
  (void) align_mp (z, &expo, digits);
  MP_EXPONENT (z) = (MP_T) expo;
  if (sign_x != 0) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  check_mp_exp (p, z);
}

//! @brief PROC quad mp = (LONG LONG REAL) LONG LONG REAL

void genie_quad_mp (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  QUAD_T u = mp_to_quad_real (p, z, digits);
  quad_real_to_mp (p, z, u, digits);
}

#endif
