//! @file mp-mpfr.c
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
#include "a68g-mp.h"
#include "a68g-double.h"

#if (A68_LEVEL >= 3)

#if defined (HAVE_GNU_MPFR)

#define DEFAULT GMP_RNDN

#define MPFR_REAL_BITS (REAL_MANT_DIG)
#define MPFR_LONG_REAL_BITS (FLT128_MANT_DIG)
#define MPFR_MP_BITS (MANT_BITS (mpfr_digits ()))

#define NO_MPFR ((mpfr_ptr) NULL)

#define CHECK_MPFR(p, z) PRELUDE_ERROR (mpfr_number_p (z) == 0, (p), ERROR_MATH, M_LONG_LONG_REAL)

static void zeroin_mpfr (NODE_T *, mpfr_t *, mpfr_t, mpfr_t, mpfr_t, int (*)(mpfr_t, const mpfr_t, mpfr_rnd_t));

//! @brief Decimal digits in mpfr significand.

size_t mpfr_digits (void)
{
  return (long_mp_digits () * LOG_MP_RADIX);
}

//! @brief Convert mp to mpfr.

void mp_to_mpfr (NODE_T * p, MP_T * z, mpfr_t * x, int digits)
{
// This routine looks a lot like "strtod".
  (void) p;
  mpfr_set_ui (*x, 0, DEFAULT);
  if (MP_EXPONENT (z) * (MP_T) LOG_MP_RADIX > (MP_T) REAL_MIN_10_EXP) {
    int j, expo;
    BOOL_T neg = MP_DIGIT (z, 1) < 0;
    mpfr_t term, W;
    mpfr_inits2 (MPFR_MP_BITS, term, W, NO_MPFR);
    MP_DIGIT (z, 1) = ABS (MP_DIGIT (z, 1));
    expo = (int) (MP_EXPONENT (z) * LOG_MP_RADIX);
    mpfr_set_ui (W, 10, DEFAULT);
    mpfr_pow_si (W, W, expo, DEFAULT);
    for (j = 1; j <= digits; j++) {
      mpfr_set_d (term, MP_DIGIT (z, j), DEFAULT);
      mpfr_mul (term, term, W, DEFAULT);
      mpfr_add (*x, *x, term, DEFAULT);
      mpfr_div_ui (W, W, MP_RADIX, DEFAULT);
    }
    if (neg) {
      mpfr_neg (*x, *x, DEFAULT);
    }
  }
}

//! @brief Convert mpfr to mp number.

MP_T *mpfr_to_mp (NODE_T * p, MP_T * z, mpfr_t * x, int digits)
{
  int j, k, sign_x, sum, W;
  mpfr_t u, v, t;
  SET_MP_ZERO (z, digits);
  if (mpfr_zero_p (*x)) {
    return z;
  }
  sign_x = mpfr_sgn (*x);
  mpfr_inits2 (MPFR_MP_BITS, t, u, v, NO_MPFR);
// Scale to [0, 0.1>.
// a = ABS (x);
  mpfr_set (u, *x, DEFAULT);
  mpfr_abs (u, u, DEFAULT);
// expo = (int) log10q (a);
  mpfr_log10 (v, u, DEFAULT);
  INT_T expo = mpfr_get_si (v, DEFAULT);
// v /= ten_up_double (expo);
  mpfr_set_ui (v, 10, DEFAULT);
  mpfr_pow_si (v, v, expo, DEFAULT);
  mpfr_div (u, u, v, DEFAULT);
  expo--;
  if (mpfr_cmp_ui (u, 1) >= 0) {
    mpfr_div_ui (u, u, 10, DEFAULT);
    expo++;
  }
// Transport digits of x to the mantissa of z.
  sum = 0;
  W = (MP_RADIX / 10);
  for (k = 0, j = 1; j <= digits && k < mpfr_digits (); k++) {
    mpfr_mul_ui (t, u, 10, DEFAULT);
    mpfr_floor (v, t);
    mpfr_frac (u, t, DEFAULT);
    sum += W * mpfr_get_d (v, DEFAULT);
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
  MP_DIGIT (z, 1) *= sign_x;
  check_mp_exp (p, z);
  mpfr_clear (t);
  mpfr_clear (u);
  mpfr_clear (v);
  return z;
}

//! @brief PROC long long mpfr = (LONG LONG REAL) LONG LONG REAL

void genie_mpfr_mp (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  mpfr_t u;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  mpfr_init2 (u, MPFR_MP_BITS);
  mp_to_mpfr (p, z, &u, digits);
  mpfr_out_str (stdout, 10, 0, u, DEFAULT);
  CHECK_MPFR (p, u);
  mpfr_to_mp (p, z, &u, digits);
  mpfr_clear (u);
}

//! @brief mpfr_beta_inc

void mpfr_beta_inc (mpfr_t I, mpfr_t s, mpfr_t t, mpfr_t x, mpfr_rnd_t rnd)
{
// Incomplete beta function I{x}(s, t).
// From a continued fraction, see dlmf.nist.gov/8.17; Lentz's algorithm.
  errno = EDOM;                 // Until proven otherwise
//mpfr_printf ("%.128Rf", x);
  if (mpfr_cmp_d (x, 0) < 0 || mpfr_cmp_d (x, 1) > 0) {
    mpfr_set_nan (I);
  } else {
    mpfr_t a, b, c, d, e, F, T, W;
    int N, m, cont = A68_TRUE;
    mpfr_prec_t lim = 2 * mpfr_get_prec (x);
    mpfr_inits2 (MPFR_MP_BITS, a, b, c, d, e, F, T, W, NO_MPFR);
// Rapid convergence when x < (s+1)/(s+t+2)
    mpfr_add_d (a, s, 1, rnd);
    mpfr_add (b, s, t, rnd);
    mpfr_add_d (b, b, 2, rnd);
    mpfr_div (c, a, b, rnd);
// Recursion when x > (s+1)/(s+t+2)
    if (mpfr_cmp (x, c) > 0) {
// B{x}(s, t) = 1 - B{1-x}(t, s)
      mpfr_d_sub (d, 1, x, rnd);
      mpfr_beta_inc (I, t, s, d, rnd);
      mpfr_d_sub (I, 1, I, rnd);
      mpfr_clears (a, b, c, d, e, F, T, W, NO_MPFR);
      return;
    }
// Lentz's algorithm for continued fraction.
    mpfr_set_d (W, 1, rnd);
    mpfr_set_d (F, 1, rnd);
    mpfr_set_d (c, 1, rnd);
    mpfr_set_d (d, 0, rnd);
    for (N = 0, m = 0; cont && N < lim; N++) {
      if (N == 0) {
// d := 1
        mpfr_set_d (T, 1, rnd);
      } else if (N % 2 == 0) {
// d{2m} := x m(t-m)/((s+2m-1)(s+2m))
        mpfr_sub_si (a, t, m, rnd);
        mpfr_mul_si (a, a, m, rnd);
        mpfr_mul (a, a, x, rnd);
        mpfr_add_si (b, s, m, rnd);
        mpfr_add_si (b, b, m, rnd);
        mpfr_set (e, b, rnd);
        mpfr_sub_d (b, b, 1, rnd);
        mpfr_mul (b, b, e, rnd);
        mpfr_div (T, a, b, rnd);
      } else {
// d{2m+1} := -x (s+m)(s+t+m)/((s+2m+1)(s+2m))
        mpfr_add_si (e, s, m, rnd);
        mpfr_add (T, e, t, rnd);
        mpfr_mul (a, e, T, rnd);
        mpfr_mul (a, a, x, rnd);
        mpfr_add_si (b, s, m, rnd);
        mpfr_add_si (b, b, m, rnd);
        mpfr_set (e, b, rnd);
        mpfr_add_d (b, b, 1, rnd);
        mpfr_mul (b, b, e, rnd);
        mpfr_div (T, a, b, rnd);
        mpfr_neg (T, T, rnd);
        m++;
      }
      mpfr_mul (e, T, d, rnd);
      mpfr_add_d (d, e, 1, rnd);
      mpfr_d_div (d, 1, d, rnd);
      mpfr_div (e, T, c, rnd);
      mpfr_add_d (c, e, 1, rnd);
      mpfr_mul (F, F, c, rnd);
      mpfr_mul (F, F, d, rnd);
      if (mpfr_cmp (F, W) == 0) {
        cont = A68_FALSE;
        errno = 0;
      } else {
        mpfr_set (W, F, rnd);
      }
    }
// I{x}(s,t)=x^s(1-x)^t / a / B(s,t) F
    mpfr_pow (a, x, s, rnd);
    mpfr_d_sub (b, 1, x, rnd);
    mpfr_pow (b, b, t, rnd);
    mpfr_mul (a, a, b, rnd);
    mpfr_beta (W, s, t, rnd);
    mpfr_sub_d (F, F, 1, rnd);
    mpfr_mul (b, F, a, rnd);
    mpfr_div (b, b, W, rnd);
    mpfr_div (b, b, s, rnd);
    mpfr_set (I, b, rnd);
    mpfr_clears (a, b, c, d, e, F, T, W, NO_MPFR);
  }
}

//! @brief PROC long long erf = (LONG LONG REAL) LONG LONG REAL

void genie_mpfr_erf_mp (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  mpfr_t u;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  mpfr_init2 (u, MPFR_MP_BITS);
  mp_to_mpfr (p, z, &u, digits);
  mpfr_erf (u, u, DEFAULT);
  CHECK_MPFR (p, u);
  mpfr_to_mp (p, z, &u, digits);
  mpfr_clear (u);
}

//! @brief PROC long long erfc = (LONG LONG REAL) LONG LONG REAL

void genie_mpfr_erfc_mp (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  mpfr_t u;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  mpfr_init2 (u, MPFR_MP_BITS);
  mp_to_mpfr (p, z, &u, digits);
  mpfr_erfc (u, u, DEFAULT);
  CHECK_MPFR (p, u);
  mpfr_to_mp (p, z, &u, digits);
  mpfr_clear (u);
}

//! @brief PROC long long inverf = (LONG LONG REAL) LONG LONG REAL

void genie_mpfr_inverf_mp (NODE_T * _p_)
{
  MOID_T *mode = MOID (_p_);
  int digits = DIGITS (mode), size = SIZE (mode);
  REAL_T x0;
  mpfr_t a, b, u, y;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  A68 (f_entry) = _p_;
  mpfr_inits2 (MPFR_MP_BITS, a, b, u, y, NO_MPFR);
  mp_to_mpfr (_p_, z, &y, digits);
  x0 = a68_inverf (mp_to_real (_p_, z, digits));
//  a = z - 1e-9;
//  b = z + 1e-9;
  mpfr_set_d (a, x0 - 1e-9, DEFAULT);
  mpfr_set_d (b, x0 + 1e-9, DEFAULT);
  zeroin_mpfr (_p_, &u, a, b, y, mpfr_erf);
  MATH_RTE (_p_, errno != 0, M_LONG_LONG_REAL, NO_TEXT);
  mpfr_to_mp (_p_, z, &u, digits);
  mpfr_clears (a, b, u, y, NO_MPFR);
}

//! @brief PROC long long inverfc = (LONG LONG REAL) LONG LONG REAL

void genie_mpfr_inverfc_mp (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  ADDR_T pop_sp = A68_SP;
  int digits = DIGITS (mode), size = SIZE (mode);
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  one_minus_mp (p, z, z, digits);
  A68_SP = pop_sp;
  genie_inverf_mp (p);
}

//! @brief PROC long long gamma = (LONG LONG REAL) LONG LONG REAL

void genie_gamma_mpfr (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  mpfr_t u;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  mpfr_init2 (u, MPFR_MP_BITS);
  mp_to_mpfr (p, z, &u, digits);
  mpfr_gamma (u, u, DEFAULT);
  CHECK_MPFR (p, u);
  mpfr_to_mp (p, z, &u, digits);
  mpfr_clear (u);
}

//! @brief PROC long long ln gamma = (LONG LONG REAL) LONG LONG REAL

void genie_lngamma_mpfr (NODE_T * p)
{
  MOID_T *mode = MOID (p);
  int digits = DIGITS (mode);
  int size = SIZE (mode);
  mpfr_t u;
  MP_T *z = (MP_T *) STACK_OFFSET (-size);
  mpfr_init2 (u, MPFR_MP_BITS);
  mp_to_mpfr (p, z, &u, digits);
  mpfr_lngamma (u, u, DEFAULT);
  CHECK_MPFR (p, u);
  mpfr_to_mp (p, z, &u, digits);
  mpfr_clear (u);
}

void genie_gamma_inc_real_mpfr (NODE_T * p)
{
  A68_REAL x, s;
  mpfr_t xx, ss;
  POP_OBJECT (p, &x, A68_REAL);
  POP_OBJECT (p, &s, A68_REAL);
  mpfr_inits2 (MPFR_LONG_REAL_BITS, ss, xx, NO_MPFR);
  mpfr_set_d (xx, VALUE (&x), DEFAULT);
  mpfr_set_d (ss, VALUE (&s), DEFAULT);
  mpfr_gamma_inc (ss, ss, xx, DEFAULT);
  CHECK_MPFR (p, ss);
  PUSH_VALUE (p, mpfr_get_d (ss, DEFAULT), A68_REAL);
  mpfr_clears (ss, xx, NO_MPFR);
}

void genie_gamma_inc_real_16_mpfr (NODE_T * p)
{
  A68_LONG_REAL x, s;
  mpfr_t xx, ss;
  POP_OBJECT (p, &x, A68_LONG_REAL);
  POP_OBJECT (p, &s, A68_LONG_REAL);
  mpfr_inits2 (MPFR_LONG_REAL_BITS, ss, xx, NO_MPFR);
  mpfr_set_float128 (xx, VALUE (&x).f, DEFAULT);
  mpfr_set_float128 (ss, VALUE (&s).f, DEFAULT);
  mpfr_gamma_inc (ss, ss, xx, DEFAULT);
  CHECK_MPFR (p, ss);
  PUSH_VALUE (p, dble (mpfr_get_float128 (ss, DEFAULT)), A68_LONG_REAL);
  mpfr_clears (ss, xx, NO_MPFR);
}

void genie_gamma_inc_mpfr (NODE_T * p)
{
  int digits = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *s = (MP_T *) STACK_OFFSET (-2 * size);
  mpfr_t xx, ss;
  A68_SP -= size;
  mpfr_inits2 (MPFR_MP_BITS, ss, xx, NO_MPFR);
  mp_to_mpfr (p, x, &xx, digits);
  mp_to_mpfr (p, s, &ss, digits);
  mpfr_gamma_inc (ss, ss, xx, DEFAULT);
  CHECK_MPFR (p, ss);
  mpfr_to_mp (p, s, &ss, digits);
  mpfr_clears (ss, xx, NO_MPFR);
}

void genie_beta_mpfr (NODE_T * p)
{
  int digits = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *s = (MP_T *) STACK_OFFSET (-2 * size);
  mpfr_t xx, ss;
  A68_SP -= size;
  mpfr_inits2 (MPFR_MP_BITS, ss, xx, NO_MPFR);
  mp_to_mpfr (p, x, &xx, digits);
  mp_to_mpfr (p, s, &ss, digits);
  mpfr_beta (ss, ss, xx, DEFAULT);
  CHECK_MPFR (p, ss);
  mpfr_to_mp (p, s, &ss, digits);
  mpfr_clears (ss, xx, NO_MPFR);
}

void genie_ln_beta_mpfr (NODE_T * p)
{
  int digits = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *b = (MP_T *) STACK_OFFSET (-size);
  MP_T *a = (MP_T *) STACK_OFFSET (-2 * size);
  mpfr_t bb, aa, yy, zz;
  A68_SP -= size;
  mpfr_inits2 (MPFR_MP_BITS, aa, bb, yy, zz, NO_MPFR);
  mp_to_mpfr (p, b, &bb, digits);
  mp_to_mpfr (p, a, &aa, digits);
  mpfr_lngamma (zz, aa, DEFAULT);
  mpfr_lngamma (yy, bb, DEFAULT);
  mpfr_add (zz, zz, yy, DEFAULT);
  mpfr_add (yy, aa, bb, DEFAULT);
  mpfr_lngamma (yy, yy, DEFAULT);
  mpfr_sub (aa, zz, yy, DEFAULT);
  CHECK_MPFR (p, aa);
  mpfr_to_mp (p, a, &aa, digits);
  mpfr_clears (aa, bb, yy, zz, NO_MPFR);
}

void genie_beta_inc_mpfr (NODE_T * p)
{
  int digits = DIGITS (MOID (p)), size = SIZE (MOID (p));
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *t = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *s = (MP_T *) STACK_OFFSET (-3 * size);
  mpfr_t xx, ss, tt;
  A68_SP -= 2 * size;
  mpfr_inits2 (MPFR_MP_BITS, ss, tt, xx, NO_MPFR);
  mp_to_mpfr (p, x, &xx, digits);
  mp_to_mpfr (p, s, &ss, digits);
  mp_to_mpfr (p, t, &tt, digits);
  mpfr_beta_inc (ss, ss, tt, xx, DEFAULT);
  CHECK_MPFR (p, ss);
  mpfr_to_mp (p, s, &ss, digits);
  mpfr_clears (ss, tt, xx, NO_MPFR);
}

//! @brief zeroin

static void zeroin_mpfr (NODE_T * _p_, mpfr_t * z, mpfr_t a, mpfr_t b, mpfr_t y, int (*f) (mpfr_t, const mpfr_t, mpfr_rnd_t))
{
// 'zeroin'
// MCA 2310 in 'ALGOL 60 Procedures in Numerical Algebra' by Th.J. Dekker
  int sign, its = 5;
  BOOL_T go_on = A68_TRUE;
  mpfr_t c, fa, fb, fc, tolb, eps, p, q, v, w;
  mpfr_inits2 (MPFR_MP_BITS, c, fa, fb, fc, tolb, eps, p, q, v, w, NO_MPFR);
  mpfr_set_ui (eps, 10, DEFAULT);
  mpfr_pow_si (eps, eps, -(mpfr_digits () - 2), DEFAULT);
  f (fa, a, DEFAULT);
  mpfr_sub (fa, fa, y, DEFAULT);
  f (fb, b, DEFAULT);
  mpfr_sub (fb, fb, y, DEFAULT);
  mpfr_set (c, a, DEFAULT);
  mpfr_set (fc, fa, DEFAULT);
  while (go_on && (its--) > 0) {
    mpfr_abs (v, fc, DEFAULT);
    mpfr_abs (w, fb, DEFAULT);
    if (mpfr_cmp (v, w) < 0) {
      mpfr_set (a, b, DEFAULT);
      mpfr_set (fa, fb, DEFAULT);
      mpfr_set (b, c, DEFAULT);
      mpfr_set (fb, fc, DEFAULT);
      mpfr_set (c, a, DEFAULT);
      mpfr_set (fc, fa, DEFAULT);
    }
    mpfr_abs (tolb, b, DEFAULT);
    mpfr_add_ui (tolb, tolb, 1, DEFAULT);
    mpfr_mul (tolb, tolb, eps, DEFAULT);
    mpfr_add (w, c, b, DEFAULT);
    mpfr_div_2ui (w, w, 1, DEFAULT);
    mpfr_sub (v, w, b, DEFAULT);
    mpfr_abs (v, v, DEFAULT);
    go_on = mpfr_cmp (v, tolb) > 0;
    if (go_on) {
      mpfr_sub (p, b, a, DEFAULT);
      mpfr_mul (p, p, fb, DEFAULT);
      mpfr_sub (q, fa, fb, DEFAULT);
      if (mpfr_cmp_ui (p, 0) < 0) {
        mpfr_neg (p, p, DEFAULT);
        mpfr_neg (q, q, DEFAULT);
      }
      mpfr_set (a, b, DEFAULT);
      mpfr_set (fa, fb, DEFAULT);
      mpfr_abs (v, q, DEFAULT);
      mpfr_mul (v, v, tolb, DEFAULT);
      if (mpfr_cmp (p, v) <= 0) {
        if (mpfr_cmp (c, b) > 0) {
          mpfr_add (b, b, tolb, DEFAULT);
        } else {
          mpfr_sub (b, b, tolb, DEFAULT);
        }
      } else {
        mpfr_sub (v, w, b, DEFAULT);
        mpfr_mul (v, v, q, DEFAULT);
        if (mpfr_cmp (p, v) < 0) {
          mpfr_div (v, p, q, DEFAULT);
          mpfr_add (b, v, b, DEFAULT);
        } else {
          mpfr_set (b, w, DEFAULT);
        }
      }
      f (fb, b, DEFAULT);
      mpfr_sub (fb, fb, y, DEFAULT);
      sign = mpfr_sgn (fb) + mpfr_sgn (fc);
      if (ABS (sign) == 2) {
        mpfr_set (c, a, DEFAULT);
        mpfr_set (fc, fa, DEFAULT);
      }
    }
  }
  CHECK_MPFR (_p_, b);
  mpfr_set (*z, b, DEFAULT);
  mpfr_clears (c, fa, fb, fc, tolb, eps, p, q, v, w, NO_MPFR);
}

#endif

#endif
