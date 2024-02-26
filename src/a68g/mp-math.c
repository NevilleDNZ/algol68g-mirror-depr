//! @file mp-math.c
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
//! [LONG] LONG REAL math functions.

#include "a68g.h"

#include "a68g-double.h"
#include "a68g-genie.h"
#include "a68g-mp.h"
#include "a68g-prelude.h"

//! @brief Test on |"z"| > 0.001 for argument reduction in "sin" and "exp".

static inline BOOL_T eps_mp (MP_T * z, int digs)
{
  if (MP_DIGIT (z, 1) == 0) {
    return A68_FALSE;
  } else if (MP_EXPONENT (z) > -1) {
    return A68_TRUE;
  } else if (MP_EXPONENT (z) < -1) {
    return A68_FALSE;
  } else {
// More or less optimised for LONG and default LONG LONG precisions.
    return (BOOL_T) (digs <= 10 ? ABS (MP_DIGIT (z, 1)) > 0.01 * MP_RADIX : ABS (MP_DIGIT (z, 1)) > 0.001 * MP_RADIX);
  }
}

//! @brief PROC (LONG REAL) LONG REAL sqrt

MP_T *sqrt_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) == 0) {
    A68_SP = pop_sp;
    SET_MP_ZERO (z, digs);
    return z;
  }
  if (MP_DIGIT (x, 1) < 0) {
    A68_SP = pop_sp;
    errno = EDOM;
    return NaN_MP;
  }
  int gdigs = FUN_DIGITS (digs), hdigs;
  BOOL_T reciprocal = A68_FALSE;
  MP_T *z_g = nil_mp (p, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *tmp = nil_mp (p, gdigs);
// Scaling for small x; sqrt (x) = 1 / sqrt (1 / x).
  if ((reciprocal = (BOOL_T) (MP_EXPONENT (x_g) < 0)) == A68_TRUE) {
    (void) rec_mp (p, x_g, x_g, gdigs);
  }
  if (ABS (MP_EXPONENT (x_g)) >= 2) {
// For extreme arguments we want accurate results as well.
    int expo = (int) MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) (expo % 2);
    (void) sqrt_mp (p, z_g, x_g, gdigs);
    MP_EXPONENT (z_g) += (MP_T) (expo / 2);
  } else {
// Argument is in range. Estimate the root as double.
#if (A68_LEVEL >= 3)
    DOUBLE_T x_d = mp_to_double (p, x_g, gdigs);
    (void) double_to_mp (p, z_g, sqrt_double (x_d), gdigs);
#else
    REAL_T x_d = mp_to_real (p, x_g, gdigs);
    (void) real_to_mp (p, z_g, sqrt (x_d), gdigs);
#endif
// Newton's method: x<n+1> = (x<n> + a / x<n>) / 2.
    int decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      hdigs = MINIMUM (1 + decimals / LOG_MP_RADIX, gdigs);
      (void) div_mp (p, tmp, x_g, z_g, hdigs);
      (void) add_mp (p, tmp, z_g, tmp, hdigs);
      (void) half_mp (p, z_g, tmp, hdigs);
    } while (decimals < 2 * gdigs * LOG_MP_RADIX);
  }
  if (reciprocal) {
    (void) rec_mp (p, z_g, z_g, digs);
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL curt

MP_T *curt_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) == 0) {
    A68_SP = pop_sp;
    SET_MP_ZERO (z, digs);
    return z;
  }
  BOOL_T change_sign = A68_FALSE;
  if (MP_DIGIT (x, 1) < 0) {
    change_sign = A68_TRUE;
    MP_DIGIT (x, 1) = -MP_DIGIT (x, 1);
  }
  int gdigs = FUN_DIGITS (digs), hdigs;
  BOOL_T reciprocal = A68_FALSE;
  MP_T *z_g = nil_mp (p, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *tmp = nil_mp (p, gdigs);
// Scaling for small x; curt (x) = 1 / curt (1 / x).
  if ((reciprocal = (BOOL_T) (MP_EXPONENT (x_g) < 0)) == A68_TRUE) {
    (void) rec_mp (p, x_g, x_g, gdigs);
  }
  if (ABS (MP_EXPONENT (x_g)) >= 3) {
// For extreme arguments we want accurate results as well.
    int expo = (int) MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) (expo % 3);
    (void) curt_mp (p, z_g, x_g, gdigs);
    MP_EXPONENT (z_g) += (MP_T) (expo / 3);
  } else {
// Argument is in range. Estimate the root as double.
    int decimals;
#if (A68_LEVEL >= 3)
    DOUBLE_T x_d = mp_to_double (p, x_g, gdigs);
    (void) double_to_mp (p, z_g, cbrt_double (x_d), gdigs);
#else
    REAL_T x_d = mp_to_real (p, x_g, gdigs);
    (void) real_to_mp (p, z_g, cbrt (x_d), gdigs);
#endif
// Newton's method: x<n+1> = (2 x<n> + a / x<n> ^ 2) / 3.
    decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      hdigs = MINIMUM (1 + decimals / LOG_MP_RADIX, gdigs);
      (void) mul_mp (p, tmp, z_g, z_g, hdigs);
      (void) div_mp (p, tmp, x_g, tmp, hdigs);
      (void) add_mp (p, tmp, z_g, tmp, hdigs);
      (void) add_mp (p, tmp, z_g, tmp, hdigs);
      (void) div_mp_digit (p, z_g, tmp, (MP_T) 3, hdigs);
    } while (decimals < gdigs * LOG_MP_RADIX);
  }
  if (reciprocal) {
    (void) rec_mp (p, z_g, z_g, digs);
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
// Exit.
  A68_SP = pop_sp;
  if (change_sign) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL hypot 

MP_T *hypot_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
// sqrt (x^2 + y^2).
  ADDR_T pop_sp = A68_SP;
  MP_T *t = nil_mp (p, digs);
  MP_T *u = nil_mp (p, digs);
  MP_T *v = nil_mp (p, digs);
  (void) move_mp (u, x, digs);
  (void) move_mp (v, y, digs);
  MP_DIGIT (u, 1) = ABS (MP_DIGIT (u, 1));
  MP_DIGIT (v, 1) = ABS (MP_DIGIT (v, 1));
  if (IS_ZERO_MP (u)) {
    (void) move_mp (z, v, digs);
  } else if (IS_ZERO_MP (v)) {
    (void) move_mp (z, u, digs);
  } else {
    SET_MP_ONE (t, digs);
    (void) sub_mp (p, z, u, v, digs);
    if (MP_DIGIT (z, 1) > 0) {
      (void) div_mp (p, z, v, u, digs);
      (void) mul_mp (p, z, z, z, digs);
      (void) add_mp (p, z, t, z, digs);
      (void) sqrt_mp (p, z, z, digs);
      (void) mul_mp (p, z, u, z, digs);
    } else {
      (void) div_mp (p, z, u, v, digs);
      (void) mul_mp (p, z, z, z, digs);
      (void) add_mp (p, z, t, z, digs);
      (void) sqrt_mp (p, z, z, digs);
      (void) mul_mp (p, z, v, z, digs);
    }
  }
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL exp

MP_T *exp_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Argument is reduced by using exp (z / (2 ** n)) ** (2 ** n) = exp(z).
  int m, n, gdigs = FUN_DIGITS (digs);
  ADDR_T pop_sp = A68_SP;
  BOOL_T iterate;
  if (MP_DIGIT (x, 1) == 0) {
    SET_MP_ONE (z, digs);
    return z;
  }
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *sum = nil_mp (p, gdigs);
  MP_T *a68_pow = nil_mp (p, gdigs);
  MP_T *fac = nil_mp (p, gdigs);
  MP_T *tmp = nil_mp (p, gdigs);
  m = 0;
// Scale x down.
  while (eps_mp (x_g, gdigs)) {
    m++;
    (void) half_mp (p, x_g, x_g, gdigs);
  }
// Calculate Taylor sum exp (z) = 1 + z / 1 ! + z ** 2 / 2 ! + ..
  SET_MP_ONE (sum, gdigs);
  (void) add_mp (p, sum, sum, x_g, gdigs);
  (void) mul_mp (p, a68_pow, x_g, x_g, gdigs);
  (void) half_mp (p, tmp, a68_pow, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 6, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 24, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 120, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 720, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 5040, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 40320, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 362880, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) set_mp (fac, (MP_T) (MP_T) 3628800, 0, gdigs);
  n = 10;
  iterate = (BOOL_T) (MP_DIGIT (a68_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68_pow, fac, gdigs);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (sum) - gdigs)) {
      iterate = A68_FALSE;
    } else {
      (void) add_mp (p, sum, sum, tmp, gdigs);
      (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
      n++;
      (void) mul_mp_digit (p, fac, fac, (MP_T) n, gdigs);
    }
  }
// Square exp (x) up.
  while (m--) {
    (void) mul_mp (p, sum, sum, sum, gdigs);
  }
  (void) shorten_mp (p, z, digs, sum, gdigs);
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL exp (x) - 1
// assuming "x" to be close to 0.

MP_T *expm1_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  int n, gdigs = FUN_DIGITS (digs);
  ADDR_T pop_sp = A68_SP;
  BOOL_T iterate;
  if (MP_DIGIT (x, 1) == 0) {
    SET_MP_ONE (z, digs);
    return z;
  }
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *sum = nil_mp (p, gdigs);
  MP_T *a68_pow = nil_mp (p, gdigs);
  MP_T *fac = nil_mp (p, gdigs);
  MP_T *tmp = nil_mp (p, gdigs);
// Calculate Taylor sum expm1 (z) = z / 1 ! + z ** 2 / 2 ! + ...
  (void) add_mp (p, sum, sum, x_g, gdigs);
  (void) mul_mp (p, a68_pow, x_g, x_g, gdigs);
  (void) half_mp (p, tmp, a68_pow, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 6, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 24, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 120, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 720, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 5040, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 40320, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 362880, gdigs);
  (void) add_mp (p, sum, sum, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
  (void) set_mp (fac, (MP_T) (MP_T) 3628800, 0, gdigs);
  n = 10;
  iterate = (BOOL_T) (MP_DIGIT (a68_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68_pow, fac, gdigs);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (sum) - gdigs)) {
      iterate = A68_FALSE;
    } else {
      (void) add_mp (p, sum, sum, tmp, gdigs);
      (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
      n++;
      (void) mul_mp_digit (p, fac, fac, (MP_T) n, gdigs);
    }
  }
  (void) shorten_mp (p, z, digs, sum, gdigs);
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief ln scale

MP_T *mp_ln_scale (NODE_T * p, MP_T * z, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *z_g = nil_mp (p, gdigs);
// First see if we can restore a previous calculation.
  if (gdigs <= A68_MP (mp_ln_scale_size)) {
    (void) move_mp (z_g, A68_MP (mp_ln_scale), gdigs);
  } else {
// No luck with the kept value, we generate a longer one.
    (void) set_mp (z_g, (MP_T) 1, 1, gdigs);
    (void) ln_mp (p, z_g, z_g, gdigs);
    A68_MP (mp_ln_scale) = (MP_T *) get_heap_space ((unt) SIZE_MP (gdigs));
    (void) move_mp (A68_MP (mp_ln_scale), z_g, gdigs);
    A68_MP (mp_ln_scale_size) = gdigs;
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief ln 10

MP_T *mp_ln_10 (NODE_T * p, MP_T * z, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *z_g = nil_mp (p, gdigs);
// First see if we can restore a previous calculation.
  if (gdigs <= A68_MP (mp_ln_10_size)) {
    (void) move_mp (z_g, A68_MP (mp_ln_10), gdigs);
  } else {
// No luck with the kept value, we generate a longer one.
    (void) set_mp (z_g, (MP_T) 10, 0, gdigs);
    (void) ln_mp (p, z_g, z_g, gdigs);
    A68_MP (mp_ln_10) = (MP_T *) get_heap_space ((unt) SIZE_MP (gdigs));
    (void) move_mp (A68_MP (mp_ln_10), z_g, gdigs);
    A68_MP (mp_ln_10_size) = gdigs;
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL ln

MP_T *ln_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Depending on the argument we choose either Taylor or Newton.
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  BOOL_T negative, scale;
  MP_T expo = 0;
  if (MP_DIGIT (x, 1) <= 0) {
    errno = EDOM;
    return NaN_MP;
  }
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
// We use ln (1 / x) = - ln (x).
  negative = (BOOL_T) (MP_EXPONENT (x_g) < 0);
  if (negative) {
    (void) rec_mp (p, x_g, x_g, digs);
  }
// We want correct results for extreme arguments. We scale when "x_g" exceeds
// "MP_RADIX ** +- 2", using ln (x * MP_RADIX ** n) = ln (x) + n * ln (MP_RADIX).
  scale = (BOOL_T) (ABS (MP_EXPONENT (x_g)) >= 2);
  if (scale) {
    expo = MP_EXPONENT (x_g);
    MP_EXPONENT (x_g) = (MP_T) 0;
  }
  if (MP_EXPONENT (x_g) == 0 && MP_DIGIT (x_g, 1) == 1 && MP_DIGIT (x_g, 2) == 0) {
// Taylor sum for x close to unity.
// ln (x) = (x - 1) - (x - 1) ** 2 / 2 + (x - 1) ** 3 / 3 - ...
// This is faster for small x and avoids cancellation.
    int n = 2;
    BOOL_T iterate;
    MP_T *tmp = nil_mp (p, gdigs);
    MP_T *a68_pow = nil_mp (p, gdigs);
    (void) minus_one_mp (p, x_g, x_g, gdigs);
    (void) mul_mp (p, a68_pow, x_g, x_g, gdigs);
    (void) move_mp (z_g, x_g, gdigs);
    iterate = (BOOL_T) (MP_DIGIT (a68_pow, 1) != 0);
    while (iterate) {
      (void) div_mp_digit (p, tmp, a68_pow, (MP_T) n, gdigs);
      if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - gdigs)) {
        iterate = A68_FALSE;
      } else {
        MP_DIGIT (tmp, 1) = (EVEN (n) ? -MP_DIGIT (tmp, 1) : MP_DIGIT (tmp, 1));
        (void) add_mp (p, z_g, z_g, tmp, gdigs);
        (void) mul_mp (p, a68_pow, a68_pow, x_g, gdigs);
        n++;
      }
    }
  } else {
// Newton's method: x<n+1> = x<n> - 1 + a / exp (x<n>).
    MP_T *tmp = nil_mp (p, gdigs);
// Construct an estimate.
#if (A68_LEVEL >= 3)
    (void) double_to_mp (p, z_g, log_double (mp_to_double (p, x_g, gdigs)), gdigs);
#else
    (void) real_to_mp (p, z_g, log (mp_to_real (p, x_g, gdigs)), gdigs);
#endif
    int decimals = DOUBLE_ACCURACY;
    do {
      int hdigs;
      decimals <<= 1;
      hdigs = MINIMUM (1 + decimals / LOG_MP_RADIX, gdigs);
      (void) exp_mp (p, tmp, z_g, hdigs);
      (void) div_mp (p, tmp, x_g, tmp, hdigs);
      (void) minus_one_mp (p, z_g, z_g, hdigs);
      (void) add_mp (p, z_g, z_g, tmp, hdigs);
    } while (decimals < gdigs * LOG_MP_RADIX);
  }
// Inverse scaling.
  if (scale) {
// ln (x * MP_RADIX ** n) = ln (x) + n * ln (MP_RADIX).
    MP_T *ln_base = nil_mp (p, gdigs);
    (void) mp_ln_scale (p, ln_base, gdigs);
    (void) mul_mp_digit (p, ln_base, ln_base, (MP_T) expo, gdigs);
    (void) add_mp (p, z_g, z_g, ln_base, gdigs);
  }
  if (negative) {
    MP_DIGIT (z_g, 1) = -MP_DIGIT (z_g, 1);
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL log

MP_T *log_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *ln_10 = nil_mp (p, digs);
  if (ln_mp (p, z, x, digs) == NaN_MP) {
    errno = EDOM;
    return NaN_MP;
  }
  (void) mp_ln_10 (p, ln_10, digs);
  (void) div_mp (p, z, z, ln_10, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief sinh ("z") and cosh ("z") 

MP_T *hyp_mp (NODE_T * p, MP_T * sh, MP_T * ch, MP_T * z, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *x_g = nil_mp (p, digs);
  MP_T *y_g = nil_mp (p, digs);
  MP_T *z_g = nil_mp (p, digs);
  (void) move_mp (z_g, z, digs);
  (void) exp_mp (p, x_g, z_g, digs);
  (void) rec_mp (p, y_g, x_g, digs);
  (void) add_mp (p, ch, x_g, y_g, digs);
// Avoid cancellation for sinh.
  if ((MP_DIGIT (x_g, 1) == 1 && MP_DIGIT (x_g, 2) == 0) || (MP_DIGIT (y_g, 1) == 1 && MP_DIGIT (y_g, 2) == 0)) {
    (void) expm1_mp (p, x_g, z_g, digs);
    MP_DIGIT (z_g, 1) = -MP_DIGIT (z_g, 1);
    (void) expm1_mp (p, y_g, z_g, digs);
  }
  (void) sub_mp (p, sh, x_g, y_g, digs);
  (void) half_mp (p, sh, sh, digs);
  (void) half_mp (p, ch, ch, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL sinh

MP_T *sinh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) hyp_mp (p, z_g, y_g, x_g, gdigs);
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL asinh

MP_T *asinh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (IS_ZERO_MP (x)) {
    SET_MP_ZERO (z, digs);
    return z;
  } else {
    ADDR_T pop_sp = A68_SP;
    int gdigs;
    if (MP_EXPONENT (x) >= -1) {
      gdigs = FUN_DIGITS (digs);
    } else {
// Extra precision when x^2+1 gets close to 1.
      gdigs = 2 * FUN_DIGITS (digs);
    }
    MP_T *x_g = len_mp (p, x, digs, gdigs);
    MP_T *y_g = nil_mp (p, gdigs);
    MP_T *z_g = nil_mp (p, gdigs);
    (void) mul_mp (p, z_g, x_g, x_g, gdigs);
    SET_MP_ONE (y_g, gdigs);
    (void) add_mp (p, y_g, z_g, y_g, gdigs);
    (void) sqrt_mp (p, y_g, y_g, gdigs);
    (void) add_mp (p, y_g, y_g, x_g, gdigs);
    (void) ln_mp (p, z_g, y_g, gdigs);
    if (IS_ZERO_MP (z_g)) {
      (void) move_mp (z, x, digs);
    } else {
      (void) shorten_mp (p, z, digs, z_g, gdigs);
    }
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL cosh

MP_T *cosh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) hyp_mp (p, y_g, z_g, x_g, gdigs);
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL acosh

MP_T *acosh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs;
  if (MP_DIGIT (x, 1) == 1 && MP_DIGIT (x, 2) == 0) {
// Extra precision when x^2-1 gets close to 0.
    gdigs = 2 * FUN_DIGITS (digs);
  } else {
    gdigs = FUN_DIGITS (digs);
  }
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) mul_mp (p, z_g, x_g, x_g, gdigs);
  SET_MP_ONE (y_g, gdigs);
  (void) sub_mp (p, y_g, z_g, y_g, gdigs);
  (void) sqrt_mp (p, y_g, y_g, gdigs);
  (void) add_mp (p, y_g, y_g, x_g, gdigs);
  (void) ln_mp (p, z_g, y_g, gdigs);
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL tanh

MP_T *tanh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) hyp_mp (p, y_g, z_g, x_g, gdigs);
  (void) div_mp (p, z_g, y_g, z_g, gdigs);
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL atanh

MP_T *atanh_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  SET_MP_ONE (y_g, gdigs);
  (void) add_mp (p, z_g, y_g, x_g, gdigs);
  (void) sub_mp (p, y_g, y_g, x_g, gdigs);
  (void) div_mp (p, y_g, z_g, y_g, gdigs);
  (void) ln_mp (p, z_g, y_g, gdigs);
  (void) half_mp (p, z_g, z_g, gdigs);
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL sin

MP_T *sin_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Use triple-angle relation to reduce argument.
  ADDR_T pop_sp = A68_SP;
  int m, n, gdigs = FUN_DIGITS (digs);
  BOOL_T flip, negative, iterate, even;
// We will use "pi".
  MP_T *pi = nil_mp (p, gdigs);
  MP_T *tpi = nil_mp (p, gdigs);
  MP_T *hpi = nil_mp (p, gdigs);
  (void) mp_pi (p, pi, MP_PI, gdigs);
  (void) mp_pi (p, tpi, MP_TWO_PI, gdigs);
  (void) mp_pi (p, hpi, MP_HALF_PI, gdigs);
// Argument reduction (1): sin (x) = sin (x mod 2 pi).
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  (void) mod_mp (p, x_g, x_g, tpi, gdigs);
// Argument reduction (2): sin (-x) = sin (x)
//                         sin (x) = - sin (x - pi); pi < x <= 2 pi
//                         sin (x) = sin (pi - x);   pi / 2 < x <= pi
  negative = (BOOL_T) (MP_DIGIT (x_g, 1) < 0);
  if (negative) {
    MP_DIGIT (x_g, 1) = -MP_DIGIT (x_g, 1);
  }
  MP_T *tmp = nil_mp (p, gdigs);
  (void) sub_mp (p, tmp, x_g, pi, gdigs);
  flip = (BOOL_T) (MP_DIGIT (tmp, 1) > 0);
  if (flip) {                   // x > pi
    (void) sub_mp (p, x_g, x_g, pi, gdigs);
  }
  (void) sub_mp (p, tmp, x_g, hpi, gdigs);
  if (MP_DIGIT (tmp, 1) > 0) {  // x > pi / 2
    (void) sub_mp (p, x_g, pi, x_g, gdigs);
  }
// Argument reduction (3): (follows from De Moivre's theorem)
// sin (3x) = sin (x) * (3 - 4 sin ^ 2 (x))
  m = 0;
  while (eps_mp (x_g, gdigs)) {
    m++;
    (void) div_mp_digit (p, x_g, x_g, (MP_T) 3, gdigs);
  }
// Taylor sum.
  MP_T *sqr = nil_mp (p, gdigs);
  MP_T *a68_pow = nil_mp (p, gdigs);
  MP_T *fac = nil_mp (p, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) mul_mp (p, sqr, x_g, x_g, gdigs);
  (void) mul_mp (p, a68_pow, sqr, x_g, gdigs);
  (void) move_mp (z_g, x_g, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 6, gdigs);
  (void) sub_mp (p, z_g, z_g, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, sqr, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 120, gdigs);
  (void) add_mp (p, z_g, z_g, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, sqr, gdigs);
  (void) div_mp_digit (p, tmp, a68_pow, (MP_T) 5040, gdigs);
  (void) sub_mp (p, z_g, z_g, tmp, gdigs);
  (void) mul_mp (p, a68_pow, a68_pow, sqr, gdigs);
  (void) set_mp (fac, (MP_T) 362880, 0, gdigs);
  n = 9;
  even = A68_TRUE;
  iterate = (BOOL_T) (MP_DIGIT (a68_pow, 1) != 0);
  while (iterate) {
    (void) div_mp (p, tmp, a68_pow, fac, gdigs);
    if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - gdigs)) {
      iterate = A68_FALSE;
    } else {
      if (even) {
        (void) add_mp (p, z_g, z_g, tmp, gdigs);
        even = A68_FALSE;
      } else {
        (void) sub_mp (p, z_g, z_g, tmp, gdigs);
        even = A68_TRUE;
      }
      (void) mul_mp (p, a68_pow, a68_pow, sqr, gdigs);
      (void) mul_mp_digit (p, fac, fac, (MP_T) (++n), gdigs);
      (void) mul_mp_digit (p, fac, fac, (MP_T) (++n), gdigs);
    }
  }
// Inverse scaling using sin (3x) = sin (x) * (3 - 4 sin ** 2 (x)).
// Use existing mp's for intermediates.
  (void) set_mp (fac, (MP_T) 3, 0, gdigs);
  while (m--) {
    (void) mul_mp (p, a68_pow, z_g, z_g, gdigs);
    (void) mul_mp_digit (p, a68_pow, a68_pow, (MP_T) 4, gdigs);
    (void) sub_mp (p, a68_pow, fac, a68_pow, gdigs);
    (void) mul_mp (p, z_g, a68_pow, z_g, gdigs);
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  if (negative ^ flip) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL cos

MP_T *cos_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Use cos (x) = sin (pi / 2 - x).
// Compute x mod 2 pi before subtracting to avoid cancellation.
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *hpi = nil_mp (p, gdigs);
  MP_T *tpi = nil_mp (p, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y = nil_mp (p, digs);
  (void) mp_pi (p, hpi, MP_HALF_PI, gdigs);
  (void) mp_pi (p, tpi, MP_TWO_PI, gdigs);
  (void) mod_mp (p, x_g, x_g, tpi, gdigs);
  (void) sub_mp (p, x_g, hpi, x_g, gdigs);
  (void) shorten_mp (p, y, digs, x_g, gdigs);
  (void) sin_mp (p, z, y, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL tan

MP_T *tan_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Use tan (x) = sin (x) / sqrt (1 - sin ^ 2 (x)).
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  BOOL_T negate;
  MP_T *pi = nil_mp (p, gdigs);
  MP_T *hpi = nil_mp (p, gdigs);
  (void) mp_pi (p, pi, MP_PI, gdigs);
  (void) mp_pi (p, hpi, MP_HALF_PI, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *sns = nil_mp (p, digs);
  MP_T *cns = nil_mp (p, digs);
// Argument mod pi.
  (void) mod_mp (p, x_g, x_g, pi, gdigs);
  if (MP_DIGIT (x_g, 1) >= 0) {
    (void) sub_mp (p, y_g, x_g, hpi, gdigs);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) > 0);
  } else {
    (void) add_mp (p, y_g, x_g, hpi, gdigs);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) < 0);
  }
  (void) shorten_mp (p, x, digs, x_g, gdigs);
// tan(x) = sin(x) / sqrt (1 - sin ** 2 (x)).
  (void) sin_mp (p, sns, x, digs);
  (void) mul_mp (p, cns, sns, sns, digs);
  (void) one_minus_mp (p, cns, cns, digs);
  (void) sqrt_mp (p, cns, cns, digs);
  if (div_mp (p, z, sns, cns, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  A68_SP = pop_sp;
  if (negate) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arcsin

MP_T *asin_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *y = nil_mp (p, digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) mul_mp (p, z_g, x_g, x_g, gdigs);
  (void) one_minus_mp (p, z_g, z_g, gdigs);
  if (sqrt_mp (p, z_g, z_g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  if (MP_DIGIT (z_g, 1) == 0) {
    (void) mp_pi (p, z, MP_HALF_PI, digs);
    MP_DIGIT (z, 1) = (MP_DIGIT (x_g, 1) >= 0 ? MP_DIGIT (z, 1) : -MP_DIGIT (z, 1));
    A68_SP = pop_sp;
    return z;
  }
  if (div_mp (p, x_g, x_g, z_g, gdigs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  (void) shorten_mp (p, y, digs, x_g, gdigs);
  (void) atan_mp (p, z, y, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arccos

MP_T *acos_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  BOOL_T negative = (BOOL_T) (MP_DIGIT (x, 1) < 0);
  if (MP_DIGIT (x, 1) == 0) {
    (void) mp_pi (p, z, MP_HALF_PI, digs);
    A68_SP = pop_sp;
    return z;
  }
  MP_T *y = nil_mp (p, digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  (void) mul_mp (p, z_g, x_g, x_g, gdigs);
  (void) one_minus_mp (p, z_g, z_g, gdigs);
  if (sqrt_mp (p, z_g, z_g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  if (div_mp (p, x_g, z_g, x_g, gdigs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  (void) shorten_mp (p, y, digs, x_g, gdigs);
  (void) atan_mp (p, z, y, digs);
  if (negative) {
    (void) mp_pi (p, y, MP_PI, digs);
    (void) add_mp (p, z, z, y, digs);
  }
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arctan

MP_T *atan_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Depending on the argument we choose either Taylor or Newton.
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) == 0) {
    A68_SP = pop_sp;
    SET_MP_ZERO (z, digs);
    return z;
  }
  int gdigs = FUN_DIGITS (digs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *z_g = nil_mp (p, gdigs);
  BOOL_T negative = (BOOL_T) (MP_DIGIT (x_g, 1) < 0);
  if (negative) {
    MP_DIGIT (x_g, 1) = -MP_DIGIT (x_g, 1);
  }
// For larger arguments we use atan(x) = pi/2 - atan(1/x).
  BOOL_T flip = (BOOL_T) (((MP_EXPONENT (x_g) > 0) || (MP_EXPONENT (x_g) == 0 && MP_DIGIT (x_g, 1) > 1)) && (MP_DIGIT (x_g, 1) != 0));
  if (flip) {
    (void) rec_mp (p, x_g, x_g, gdigs);
  }
  if (MP_EXPONENT (x_g) < -1 || (MP_EXPONENT (x_g) == -1 && MP_DIGIT (x_g, 1) < MP_RADIX / 100)) {
// Taylor sum for x close to zero.
// arctan (x) = x - x ** 3 / 3 + x ** 5 / 5 - x ** 7 / 7 + ..
// This is faster for small x and avoids cancellation
    int n = 3;
    BOOL_T iterate, even;
    MP_T *tmp = nil_mp (p, gdigs);
    MP_T *a68_pow = nil_mp (p, gdigs);
    MP_T *sqr = nil_mp (p, gdigs);
    (void) mul_mp (p, sqr, x_g, x_g, gdigs);
    (void) mul_mp (p, a68_pow, sqr, x_g, gdigs);
    (void) move_mp (z_g, x_g, gdigs);
    even = A68_FALSE;
    iterate = (BOOL_T) (MP_DIGIT (a68_pow, 1) != 0);
    while (iterate) {
      (void) div_mp_digit (p, tmp, a68_pow, (MP_T) n, gdigs);
      if (MP_EXPONENT (tmp) <= (MP_EXPONENT (z_g) - gdigs)) {
        iterate = A68_FALSE;
      } else {
        if (even) {
          (void) add_mp (p, z_g, z_g, tmp, gdigs);
          even = A68_FALSE;
        } else {
          (void) sub_mp (p, z_g, z_g, tmp, gdigs);
          even = A68_TRUE;
        }
        (void) mul_mp (p, a68_pow, a68_pow, sqr, gdigs);
        n += 2;
      }
    }
  } else {
// Newton's method: x<n+1> = x<n> - cos (x<n>) * (sin (x<n>) - a cos (x<n>)).
    int decimals, hdigs;
    MP_T *tmp = nil_mp (p, gdigs);
    MP_T *sns = nil_mp (p, gdigs);
    MP_T *cns = nil_mp (p, gdigs);
// Construct an estimate.
#if (A68_LEVEL >= 3)
    (void) double_to_mp (p, z_g, atan_double (mp_to_double (p, x_g, gdigs)), gdigs);
#else
    (void) real_to_mp (p, z_g, atan (mp_to_real (p, x_g, gdigs)), gdigs);
#endif
    decimals = DOUBLE_ACCURACY;
    do {
      decimals <<= 1;
      hdigs = MINIMUM (1 + decimals / LOG_MP_RADIX, gdigs);
      (void) sin_mp (p, sns, z_g, hdigs);
      (void) mul_mp (p, tmp, sns, sns, hdigs);
      (void) one_minus_mp (p, tmp, tmp, hdigs);
      (void) sqrt_mp (p, cns, tmp, hdigs);
      (void) mul_mp (p, tmp, x_g, cns, hdigs);
      (void) sub_mp (p, tmp, sns, tmp, hdigs);
      (void) mul_mp (p, tmp, tmp, cns, hdigs);
      (void) sub_mp (p, z_g, z_g, tmp, hdigs);
    } while (decimals < gdigs * LOG_MP_RADIX);
  }
  if (flip) {
    MP_T *hpi = nil_mp (p, gdigs);
    (void) sub_mp (p, z_g, mp_pi (p, hpi, MP_HALF_PI, gdigs), z_g, gdigs);
  }
  (void) shorten_mp (p, z, digs, z_g, gdigs);
  MP_DIGIT (z, 1) = (negative ? -MP_DIGIT (z, 1) : MP_DIGIT (z, 1));
// Exit.
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL atan2

MP_T *atan2_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *t = nil_mp (p, digs);
  if (MP_DIGIT (x, 1) == 0 && MP_DIGIT (y, 1) == 0) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    BOOL_T flip = (BOOL_T) (MP_DIGIT (y, 1) < 0);
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    if (IS_ZERO_MP (x)) {
      (void) mp_pi (p, z, MP_HALF_PI, digs);
    } else {
      BOOL_T flop = (BOOL_T) (MP_DIGIT (x, 1) <= 0);
      MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
      (void) div_mp (p, z, y, x, digs);
      (void) atan_mp (p, z, z, digs);
      if (flop) {
        (void) mp_pi (p, t, MP_PI, digs);
        (void) sub_mp (p, z, t, z, digs);
      }
    }
    if (flip) {
      MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
    }
  }
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL csc

MP_T *csc_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  sin_mp (p, z, x, digs);
  if (rec_mp (p, z, z, digs) == NaN_MP) {
    return NaN_MP;
  }
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL acsc

MP_T *acsc_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (rec_mp (p, z, x, digs) == NaN_MP) {
    return NaN_MP;
  }
  return asin_mp (p, z, z, digs);
}

//! @brief PROC (LONG REAL) LONG REAL sec

MP_T *sec_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  cos_mp (p, z, x, digs);
  if (rec_mp (p, z, z, digs) == NaN_MP) {
    return NaN_MP;
  }
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL asec

MP_T *asec_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (rec_mp (p, z, x, digs) == NaN_MP) {
    return NaN_MP;
  }
  return acos_mp (p, z, z, digs);
}

//! @brief PROC (LONG REAL) LONG REAL cot

MP_T *cot_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
// Use tan (x) = sin (x) / sqrt (1 - sin ^ 2 (x)).
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  BOOL_T negate;
  MP_T *pi = nil_mp (p, gdigs);
  MP_T *hpi = nil_mp (p, gdigs);
  (void) mp_pi (p, pi, MP_PI, gdigs);
  (void) mp_pi (p, hpi, MP_HALF_PI, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  MP_T *sns = nil_mp (p, digs);
  MP_T *cns = nil_mp (p, digs);
// Argument mod pi.
  (void) mod_mp (p, x_g, x_g, pi, gdigs);
  if (MP_DIGIT (x_g, 1) >= 0) {
    (void) sub_mp (p, y_g, x_g, hpi, gdigs);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) > 0);
  } else {
    (void) add_mp (p, y_g, x_g, hpi, gdigs);
    negate = (BOOL_T) (MP_DIGIT (y_g, 1) < 0);
  }
  (void) shorten_mp (p, x, digs, x_g, gdigs);
// tan(x) = sin(x) / sqrt (1 - sin ** 2 (x)).
  (void) sin_mp (p, sns, x, digs);
  (void) mul_mp (p, cns, sns, sns, digs);
  (void) one_minus_mp (p, cns, cns, digs);
  (void) sqrt_mp (p, cns, cns, digs);
  if (div_mp (p, z, cns, sns, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  A68_SP = pop_sp;
  if (negate) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arccot

MP_T *acot_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  if (rec_mp (p, f, x, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    (void) atan_mp (p, z, f, digs);
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL sindg

MP_T *sindg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) mp_pi (p, f, MP_PI_OVER_180, digs);
  (void) mul_mp (p, g, x, f, digs);
  (void) sin_mp (p, z, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL cosdg

MP_T *cosdg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) mp_pi (p, f, MP_PI_OVER_180, digs);
  (void) mul_mp (p, g, x, f, digs);
  (void) cos_mp (p, z, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL tandg

MP_T *tandg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) mp_pi (p, f, MP_PI_OVER_180, digs);
  (void) mul_mp (p, g, x, f, digs);
  if (tan_mp (p, z, g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL cotdg

MP_T *cotdg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) mp_pi (p, f, MP_PI_OVER_180, digs);
  (void) mul_mp (p, g, x, f, digs);
  if (cot_mp (p, z, g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL arcsindg

MP_T *asindg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) asin_mp (p, f, x, digs);
  (void) mp_pi (p, g, MP_180_OVER_PI, digs);
  (void) mul_mp (p, z, f, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arccosdg

MP_T *acosdg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) acos_mp (p, f, x, digs);
  (void) mp_pi (p, g, MP_180_OVER_PI, digs);
  (void) mul_mp (p, z, f, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arctandg

MP_T *atandg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  (void) atan_mp (p, f, x, digs);
  (void) mp_pi (p, g, MP_180_OVER_PI, digs);
  (void) mul_mp (p, z, f, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL arccotdg

MP_T *acotdg_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  if (acot_mp (p, f, x, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    (void) mp_pi (p, g, MP_180_OVER_PI, digs);
    (void) mul_mp (p, z, f, g, digs);
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL atan2dg

MP_T *atan2dg_mp (NODE_T * p, MP_T * z, MP_T * x, MP_T * y, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = nil_mp (p, digs);
  if (atan2_mp (p, f, x, y, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    (void) mp_pi (p, g, MP_180_OVER_PI, digs);
    (void) mul_mp (p, z, f, g, digs);
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL sinpi

MP_T *sinpi_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = lit_mp (p, 1, 0, digs);
  (void) mod_mp (p, f, x, g, digs);
  if (IS_ZERO_MP (f)) {
    SET_MP_ZERO (z, digs);
    A68_SP = pop_sp;
    return z;
  }
  (void) mp_pi (p, f, MP_PI, digs);
  (void) mul_mp (p, g, x, f, digs);
  (void) sin_mp (p, z, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL acospi

MP_T *cospi_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = lit_mp (p, 1, 0, digs);
  (void) mod_mp (p, f, x, g, digs);
  abs_mp (p, f, f, digs);
  SET_MP_HALF (g, digs);
  (void) sub_mp (p, g, f, g, digs);
  if (IS_ZERO_MP (g)) {
    SET_MP_ZERO (z, digs);
    A68_SP = pop_sp;
    return z;
  }
  (void) mp_pi (p, f, MP_PI, digs);
  (void) mul_mp (p, g, x, f, digs);
  (void) cos_mp (p, z, g, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL tanpi

MP_T *tanpi_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = lit_mp (p, 1, 0, digs);
  MP_T *h = nil_mp (p, digs);
  MP_T *half = nil_mp (p, digs);
  SET_MP_ONE (g, digs);
  (void) mod_mp (p, f, x, g, digs);
  if (IS_ZERO_MP (f)) {
    SET_MP_ZERO (z, digs);
    A68_SP = pop_sp;
    return z;
  }
  SET_MP_MINUS_HALF (half, digs);
  (void) sub_mp (p, h, f, half, digs);
  if (MP_DIGIT (h, 1) < 0) {
    (void) plus_one_mp (p, f, f, digs);
  } else {
    SET_MP_HALF (half, digs);
    (void) sub_mp (p, h, f, half, digs);
    if (MP_DIGIT (h, 1) < 0) {
      (void) minus_one_mp (p, f, f, digs);
    }
  }
  BOOL_T neg = MP_DIGIT (f, 1) < 0;
  (void) abs_mp (p, f, f, digs);
  SET_MP_QUART (g, digs);
  (void) sub_mp (p, g, f, g, digs);
  if (IS_ZERO_MP (g)) {
    if (neg) {
      SET_MP_MINUS_ONE (z, digs);
    } else {
      SET_MP_ONE (z, digs);
    }
    A68_SP = pop_sp;
    return z;
  }
  (void) mp_pi (p, f, MP_PI, digs);
  (void) mul_mp (p, g, x, f, digs);
  if (tan_mp (p, z, g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL cotpi

MP_T *cotpi_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *f = nil_mp (p, digs);
  MP_T *g = lit_mp (p, 1, 0, digs);
  MP_T *h = nil_mp (p, digs);
  MP_T *half = nil_mp (p, digs);
  (void) mod_mp (p, f, x, g, digs);
  if (IS_ZERO_MP (f)) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  SET_MP_MINUS_HALF (half, digs);
  (void) sub_mp (p, h, f, half, digs);
  if (MP_DIGIT (h, 1) < 0) {
    (void) plus_one_mp (p, f, f, digs);
  } else {
    SET_MP_HALF (half, digs);
    (void) sub_mp (p, h, f, half, digs);
    if (MP_DIGIT (h, 1) < 0) {
      (void) minus_one_mp (p, f, f, digs);
    }
  }
  BOOL_T neg = MP_DIGIT (f, 1) < 0;
  (void) abs_mp (p, f, f, digs);
  SET_MP_QUART (g, digs);
  (void) sub_mp (p, g, f, g, digs);
  if (IS_ZERO_MP (g)) {
    if (neg) {
      SET_MP_MINUS_ONE (z, digs);
    } else {
      SET_MP_ONE (z, digs);
    }
    A68_SP = pop_sp;
    return z;
  }
  (void) mp_pi (p, f, MP_PI, digs);
  (void) mul_mp (p, g, x, f, digs);
  if (cot_mp (p, z, g, digs) == NaN_MP) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
    A68_SP = pop_sp;
    return z;
  }
}
