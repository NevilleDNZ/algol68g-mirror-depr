//! @file mp-gamma.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2021 J. Marcel van der Veer <algol68g@xs4all.nl>.
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
#include "a68g-mp.h"
#include "a68g-lib.h"

//! @brief PROC (LONG REAL) LONG REAL erf

MP_T *erf_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  if (IS_ZERO_MP (x)) {
    SET_MP_ZERO (z, digs);
    return z;
  } else {
    ADDR_T pop_sp = A68_SP;
// Note we need double precision!
    int gdigs = FUN_DIGITS (2 * digs), k = 1, sign;
    BOOL_T go_on = A68_TRUE;
    MP_T *y_g = nil_mp (p, gdigs);
    MP_T *z_g = len_mp (p, x, digs, gdigs);
    sign = MP_SIGN (x);
    (void) abs_mp (p, z_g, z_g, gdigs);
    (void) set_mp (y_g, gdigs * LOG_MP_RADIX, 0, gdigs);
    (void) sqrt_mp (p, y_g, y_g, gdigs);
    (void) sub_mp (p, y_g, z_g, y_g, gdigs);
    if (MP_SIGN (y_g) >= 0) {
      SET_MP_ONE (z, digs);
    } else {
// Taylor expansion.
      MP_T *p_g = nil_mp (p, gdigs);
      MP_T *r_g = nil_mp (p, gdigs);
      MP_T *s_g = nil_mp (p, gdigs);
      MP_T *t_g = nil_mp (p, gdigs);
      MP_T *u_g = nil_mp (p, gdigs);
      (void) mul_mp (p, y_g, z_g, z_g, gdigs);
      SET_MP_ONE (s_g, gdigs);
      SET_MP_ONE (t_g, gdigs);
      for (k = 1; go_on; k++) {
        (void) mul_mp (p, t_g, y_g, t_g, gdigs);
        (void) div_mp_digit (p, t_g, t_g, (MP_T) k, gdigs);
        (void) div_mp_digit (p, u_g, t_g, (MP_T) (2 * k + 1), gdigs);
        if (EVEN (k)) {
          (void) add_mp (p, s_g, s_g, u_g, gdigs);
        } else {
          (void) sub_mp (p, s_g, s_g, u_g, gdigs);
        }
        go_on = (MP_EXPONENT (s_g) - MP_EXPONENT (u_g)) < gdigs;
      }
      (void) mul_mp (p, r_g, z_g, s_g, gdigs);
      (void) mul_mp_digit (p, r_g, r_g, 2, gdigs);
      (void) mp_pi (p, p_g, MP_SQRT_PI, gdigs);
      (void) div_mp (p, r_g, r_g, p_g, gdigs);
      (void) shorten_mp (p, z, digs, r_g, gdigs);
    }
    if (sign < 0) {
      (void) minus_mp (p, z, z, digs);
    }
    A68_SP = pop_sp;
    return z;
  }
}

//! @brief PROC (LONG REAL) LONG REAL erfc

MP_T *erfc_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  (void) erf_mp (p, z, x, digs);
  (void) one_minus_mp (p, z, z, digs);
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL inverf

MP_T *inverf_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs, sign;
  BOOL_T go_on = A68_TRUE;
// Precision adapts to the argument, but not too much.
// If this is not precise enough, you need more digs
// in your entire calculation, not just in this routine.
// Calculate an initial Newton-Raphson estimate while at it.
#if (A68_LEVEL >= 3)
  DOUBLE_T y = ABS (mp_to_real_16 (p, x, digs));
  if (y < erfq (5.0q)) {
    y = inverf_real_16 (y);
    gdigs = FUN_DIGITS (digs);
  } else {
    y = 5.0q;
    gdigs = FUN_DIGITS (2 * digs);
  }
  MP_T *z_g = nil_mp (p, gdigs);
  (void) real_16_to_mp (p, z_g, y, gdigs);
#else
  REAL_T y = ABS (mp_to_real (p, x, digs));
  if (y < erf (4.0)) {
    y = a68_inverf (y);
    gdigs = FUN_DIGITS (digs);
  } else {
    y = 4.0;
    gdigs = FUN_DIGITS (2 * digs);
  }
  MP_T *z_g = nil_mp (p, gdigs);
  (void) real_to_mp (p, z_g, y, gdigs);
#endif
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *y_g = nil_mp (p, gdigs);
  sign = MP_SIGN (x);
  (void) abs_mp (p, x_g, x_g, gdigs);
  SET_MP_ONE (y_g, gdigs);
  (void) sub_mp (p, y_g, x_g, y_g, gdigs);
  if (MP_SIGN (y_g) >= 0) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  } else {
// Newton-Raphson.
    MP_T *d_g = nil_mp (p, gdigs);
    MP_T *f_g = nil_mp (p, gdigs);
    MP_T *p_g = nil_mp (p, gdigs);
// sqrt (pi) / 2
    (void) mp_pi (p, p_g, MP_SQRT_PI, gdigs);
    (void) half_mp (p, p_g, p_g, gdigs);
// nrdigs prevents end-less iteration
    for (int nrdigs = 0; nrdigs < digs && go_on; nrdigs++) {
      (void) move_mp (y_g, z_g, gdigs);
      (void) erf_mp (p, f_g, z_g, gdigs);
      (void) sub_mp (p, f_g, f_g, x_g, gdigs);
      (void) mul_mp (p, d_g, z_g, z_g, gdigs);
      (void) minus_mp (p, d_g, d_g, gdigs);
      (void) exp_mp (p, d_g, d_g, gdigs);
      (void) div_mp (p, f_g, f_g, d_g, gdigs);
      (void) mul_mp (p, f_g, f_g, p_g, gdigs);
      (void) sub_mp (p, z_g, z_g, f_g, gdigs);
      (void) sub_mp (p, y_g, z_g, y_g, gdigs);
      if (IS_ZERO_MP (y_g)) {
        go_on = A68_FALSE;
      } else {
        go_on = ABS (MP_EXPONENT (y_g)) < digs;
      }
    }
    (void) shorten_mp (p, z, digs, z_g, gdigs);
  }
  if (sign < 0) {
    (void) minus_mp (p, z, z, digs);
  }
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL inverfc

MP_T *inverfc_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  (void) one_minus_mp (p, z, x, digs);
  (void) inverf_mp (p, z, z, digs);
  return z;
}

// Reference: 
//   John L. Spouge. "Computation of the Gamma, Digamma, and Trigamma Functions". 
//   SIAM Journal on Numerical Analysis. 31 (3) [1994]
//
// Spouge's algorithm sums terms of greatly varying magnitude.

#define GAMMA_PRECISION(z) (2 * (z))

//! brief Set up gamma coefficient table

static void mp_gamma_table (NODE_T *p, int digs)
{
  if (A68_MP (mp_gamma_size) <= 0) {
    int b = 1;
    int gdigs = GAMMA_PRECISION (digs);
    REAL_T log_lim = -digs * LOG_MP_RADIX, log_error; 
    do {
      ABEND (b >= MP_RADIX, ERROR_HIGH_PRECISION, __func__);
// error = 1 / (sqrt (b) * a68_x_up_y (2 * M_PI, b + 0.5));
      log_error = -(log10 (b) / 2 + (b + 0.5) * log10 (2 *M_PI));
      b += 1;
    } while (log_error > log_lim);
    A68_MP (mp_gamma_size) = b;
    A68_MP (mp_gam_ck) = (MP_T **) get_heap_space ((size_t) ((b + 1) * sizeof (MP_T *)));
    A68_MP (mp_gam_ck)[0] = (MP_T *) get_heap_space (SIZE_MP (gdigs));
    (void) mp_pi (p, (A68_MP (mp_gam_ck)[0]), MP_SQRT_TWO_PI, gdigs);
    ADDR_T pop_sp = A68_SP;
    MP_T *ak = nil_mp (p, gdigs);
    MP_T *db = lit_mp (p, b, 0, gdigs);
    MP_T *ck = nil_mp (p, gdigs);
    MP_T *dk = nil_mp (p, gdigs);
    MP_T *dz = nil_mp (p, gdigs);
    MP_T *hlf = nil_mp (p, gdigs);
    MP_T *fac = lit_mp (p, 1, 0, gdigs);
    SET_MP_HALF (hlf, gdigs);
    for (int k = 1; k < b; k++) {
      set_mp (dk, k, 0, gdigs);
      (void) sub_mp (p, ak, db, dk, gdigs);
      (void) sub_mp (p, dz, dk, hlf, gdigs);
      (void) pow_mp (p, ck, ak, dz, gdigs);
      (void) exp_mp (p, dz, ak, gdigs);
      (void) mul_mp (p, ck, ck, dz, gdigs);
      (void) div_mp (p, ck, ck, fac, gdigs);
      A68_MP (mp_gam_ck)[k] = (MP_T *) get_heap_space (SIZE_MP (gdigs));
      (void) move_mp ((A68_MP(mp_gam_ck)[k]), ck, gdigs);
      (void) mul_mp (p, fac, fac, dk, gdigs);
      (void) minus_mp (p, fac, fac, gdigs);
    }
    A68_SP = pop_sp;
  }
}

static MP_T *mp_spouge_sum (NODE_T *p, MP_T *sum, MP_T *x_g, int gdigs)
{
  ADDR_T pop_sp = A68_SP;
  int a = A68_MP (mp_gamma_size);
  MP_T *da = nil_mp (p, gdigs);
  MP_T *dz = nil_mp (p, gdigs);
  (void) move_mp (sum, A68_MP (mp_gam_ck)[0], gdigs);
// Sum small to large to preserve precision.
  for (int k = a - 1; k > 0; k--) {
    set_mp (da, k, 0, gdigs);
    (void) add_mp (p, dz, x_g, da, gdigs);
    (void) div_mp (p, dz, A68_MP (mp_gam_ck)[k], dz, gdigs);
    (void) add_mp (p, sum, sum, dz, gdigs);
  }
  A68_SP = pop_sp;
  return sum;
}

//! @brief PROC (LONG REAL) LONG REAL gamma

MP_T *gamma_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  mp_gamma_table (p, digs);
  int gdigs = GAMMA_PRECISION (digs);
// Set up coefficient table.
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) < 0) {
// G(1-x)G(x) = pi / sin (pi*x)
    MP_T *pi = nil_mp (p, digs);
    MP_T *sz = nil_mp (p, digs);
    MP_T *xm = nil_mp (p, digs);
    (void) mp_pi (p, pi, MP_PI, digs);
    (void) one_minus_mp (p, xm, x, digs);
    (void) gamma_mp (p, xm, xm, digs);
    (void) sinpi_mp (p, sz, x, digs);
    (void) mul_mp (p, sz, sz, xm, digs);
    (void) div_mp (p, z, pi, sz, digs);
    A68_SP = pop_sp;
    return z;
  }
  int a = A68_MP (mp_gamma_size);
// Compute Spouge's Gamma
  MP_T *sum = nil_mp (p, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  (void) minus_one_mp (p, x_g, x_g, gdigs);
  (void) mp_spouge_sum (p, sum, x_g, gdigs);
// (z+a)^(z+0.5)*exp(-(z+a)) * Sum
  MP_T *fac = nil_mp (p, gdigs);
  MP_T *dz = nil_mp (p, gdigs);
  MP_T *az = nil_mp (p, gdigs);
  MP_T *da = nil_mp (p, gdigs);
  MP_T *hlf = nil_mp (p, gdigs);
  SET_MP_HALF (hlf, gdigs);
  set_mp (da, a, 0, gdigs);
  (void) add_mp (p, az, x_g, da, gdigs);
  (void) add_mp (p, dz, x_g, hlf, gdigs);
  (void) pow_mp (p, fac, az, dz, gdigs);
  (void) minus_mp (p, az, az, gdigs);
  (void) exp_mp (p, dz, az, gdigs);
  (void) mul_mp (p, fac, fac, dz, gdigs);
  (void) mul_mp (p, fac, sum, fac, gdigs);
  (void) shorten_mp (p, z, digs, fac, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL) LONG REAL ln gamma

MP_T *lngamma_mp (NODE_T * p, MP_T * z, MP_T * x, int digs)
{
  mp_gamma_table (p, digs);
  int gdigs = GAMMA_PRECISION (digs);
// Set up coefficient table.
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) < 0) {
// G(1-x)G(x) = pi / sin (pi*x)
    MP_T *sz = nil_mp (p, digs);
    MP_T *dz = nil_mp (p, digs);
    MP_T *xm = nil_mp (p, digs);
    (void) mp_pi (p, dz, MP_LN_PI, digs);
    (void) sinpi_mp (p, sz, x, digs);
    (void) ln_mp (p, sz, sz, digs);
    (void) sub_mp (p, dz, dz, sz, digs);
    (void) one_minus_mp (p, xm, x, digs);
    (void) lngamma_mp (p, xm, xm, digs);
    (void) sub_mp (p, z, dz, xm, digs);
    A68_SP = pop_sp;
    return z;
  }
  int a = A68_MP (mp_gamma_size);
// Compute Spouge's ln Gamma
  MP_T *sum = nil_mp (p, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  (void) minus_one_mp (p, x_g, x_g, gdigs);
  (void) mp_spouge_sum (p, sum, x_g, gdigs);
// (x+0.5) * ln (x+a) - (x+a) + ln Sum
  MP_T *da = nil_mp (p, gdigs);
  MP_T *hlf = nil_mp (p, gdigs);
  SET_MP_HALF (hlf, gdigs);
  MP_T *fac = nil_mp (p, gdigs);
  MP_T *dz = nil_mp (p, gdigs);
  MP_T *az = nil_mp (p, gdigs);
  set_mp (da, a, 0, gdigs);
  (void) add_mp (p, az, x_g, da, gdigs);
  (void) ln_mp (p, fac, az, gdigs);
  (void) add_mp (p, dz, x_g, hlf, gdigs);
  (void) mul_mp (p, fac, fac, dz, gdigs);
  (void) sub_mp (p, fac, fac, az, gdigs);
  (void) ln_mp (p, dz, sum, gdigs);
  (void) add_mp (p, fac, fac, dz, gdigs);
  (void) shorten_mp (p, z, digs, fac, gdigs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL ln beta

MP_T *lnbeta_mp (NODE_T * p, MP_T * z, MP_T * a, MP_T *b, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *aa = nil_mp (p, digs);
  MP_T *bb = nil_mp (p, digs);
  MP_T *ab = nil_mp (p, digs);
  (void) lngamma_mp (p, aa, a, digs);
  (void) lngamma_mp (p, bb, b, digs);
  (void) add_mp (p, ab, a, b, digs);
  (void) lngamma_mp (p, ab, ab, digs);
  (void) add_mp (p, z, aa, bb, digs);
  (void) sub_mp (p, z, z, ab, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL, LONG REAL) LONG REAL beta

MP_T *beta_mp (NODE_T * p, MP_T * z, MP_T * a, MP_T *b, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *u = nil_mp (p, digs);
  lnbeta_mp (p, u, a, b, digs);
  exp_mp (p, z, u, digs);
  A68_SP = pop_sp;
  return z;
}

//! @brief PROC (LONG REAL, LONG REAL, LONG REAL) LONG REAL beta inc

MP_T *beta_inc_mp (NODE_T * p, MP_T * z, MP_T * s, MP_T *t, MP_T *x, int digs)
{
// Incomplete beta function I{x}(s, t).
// Continued fraction, see dlmf.nist.gov/8.17; Lentz's algorithm.
  ADDR_T pop_sp = A68_SP;
  A68_BOOL gt;
  MP_T *one = lit_mp (p, 1, 0, digs);
  gt_mp (p, &gt, x, one, digs);
  if (MP_DIGIT (x, 1) < 0 || VALUE (&gt)) {
    errno = EDOM;
    A68_SP = pop_sp;
    return NaN_MP;
  }
  if (same_mp (p, x, one, digs)) {
    SET_MP_ONE (z, digs);
    A68_SP = pop_sp;
    return z;
  }
// Rapid convergence when x <= (s+1)/((s+1)+(t+1)) or else recursion.
  {
    MP_T *u = nil_mp (p, digs), *v = nil_mp (p, digs), *w = nil_mp (p, digs);
    (void) plus_one_mp (p, u, s, digs);
    (void) plus_one_mp (p, v, t, digs);
    (void) add_mp (p, w, u, v, digs);
    (void) div_mp (p, u, u, w, digs);
    gt_mp (p, &gt, x, u, digs);
    if (VALUE (&gt)) {
// B{x}(s, t) = 1 - B{1-x}(t, s)
      (void) one_minus_mp (p, x, x, digs);
      PRELUDE_ERROR (beta_inc_mp (p, z, s, t, x, digs) == NaN_MP, p, ERROR_INVALID_ARGUMENT, MOID (p));
      (void) one_minus_mp (p, z, z, digs);
      A68_SP = pop_sp;
      return z;
    }
  }
// Lentz's algorithm for continued fraction.
  A68_SP = pop_sp;
  int gdigs = FUN_DIGITS (digs);
  const INT_T lim = gdigs * LOG_MP_RADIX;
  BOOL_T cont = A68_TRUE;
  MP_T *F = lit_mp (p, 1, 0, gdigs);
  MP_T *T = lit_mp (p, 1, 0, gdigs);
  MP_T *W = lit_mp (p, 1, 0, gdigs);
  MP_T *c = lit_mp (p, 1, 0, gdigs);
  MP_T *d = nil_mp (p, gdigs);
  MP_T *m = nil_mp (p, gdigs);
  MP_T *s_g = len_mp (p, s, digs, gdigs);
  MP_T *t_g = len_mp (p, t, digs, gdigs);
  MP_T *x_g = len_mp (p, x, digs, gdigs);
  MP_T *u = lit_mp (p, 1, 0, gdigs);
  MP_T *v = nil_mp (p, gdigs);
  MP_T *w = nil_mp (p, gdigs);
  for (INT_T N = 0; cont && N < lim; N++) {
    if (N == 0) {
      SET_MP_ONE (T, gdigs);
    } else if (N % 2 == 0) {
// d{2m} := x m(t-m)/((s+2m-1)(s+2m))
// T = x * m * (t - m) / (s + 2.0q * m - 1.0q) / (s + 2.0q * m);
      (void) sub_mp (p, u, t_g, m, gdigs);
      (void) mul_mp (p, u, u, m, gdigs);
      (void) mul_mp (p, u, u, x_g, gdigs);
      (void) add_mp (p, v, m, m, gdigs);
      (void) add_mp (p, v, s_g, v, gdigs);
      (void) minus_one_mp (p, v, v, gdigs);
      (void) add_mp (p, w, m, m, gdigs);
      (void) add_mp (p, w, s_g, w, gdigs);
      (void) div_mp (p, T, u, v, gdigs);
      (void) div_mp (p, T, T, w, gdigs);
    } else {
// d{2m+1} := -x (s+m)(s+t+m)/((s+2m+1)(s+2m))
// T = -x * (s + m) * (s + t + m) / (s + 2.0q * m + 1.0q) / (s + 2.0q * m);
      (void) add_mp (p, u, s_g, m, gdigs);
      (void) add_mp (p, v, u, t_g, gdigs);
      (void) mul_mp (p, u, u, v, gdigs);
      (void) mul_mp (p, u, u, x_g, gdigs);
      (void) minus_mp (p, u, u, gdigs);
      (void) add_mp (p, v, m, m, gdigs);
      (void) add_mp (p, v, s_g, v, gdigs);
      (void) plus_one_mp (p, v, v, gdigs);
      (void) add_mp (p, w, m, m, gdigs);
      (void) add_mp (p, w, s_g, w, gdigs);
      (void) div_mp (p, T, u, v, gdigs);
      (void) div_mp (p, T, T, w, gdigs);
      (void) plus_one_mp (p, m, m, gdigs);
    }
// d = 1.0q / (T * d + 1.0q);
    (void) mul_mp (p, d, T, d, gdigs);
    (void) plus_one_mp (p, d, d, gdigs);
    (void) rec_mp (p, d, d, gdigs);
// c = T / c + 1.0q;
    (void) div_mp (p, c, T, c, gdigs);
    (void) plus_one_mp (p, c, c, gdigs);
// F *= c * d;
    (void) mul_mp (p, F, F, c, gdigs);
    (void) mul_mp (p, F, F, d, gdigs);
    if (same_mp (p, F, W, gdigs)) {
      cont = A68_FALSE;
    } else {
      (void) move_mp (W, F, gdigs);
    }
  }
  minus_one_mp (p, F, F, gdigs);
// I{x}(s,t)=x^s(1-x)^t / s / B(s,t) F
  (void) pow_mp (p, u, x_g, s_g, gdigs);
  (void) one_minus_mp (p, v, x_g, gdigs);
  (void) pow_mp (p, v, v, t_g, gdigs);
  (void) beta_mp (p, w, s_g, t_g, gdigs);
  (void) mul_mp (p, m, u, v, gdigs);
  (void) div_mp (p, m, m, s_g, gdigs);
  (void) div_mp (p, m, m, w, gdigs);
  (void) mul_mp (p, m, m, F, gdigs);
  (void) shorten_mp (p, z, digs, m, gdigs);
  A68_SP = pop_sp;
  return z;
}
