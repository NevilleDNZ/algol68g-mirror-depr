//! @file single-math.c
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
//
// References:
//
//   Milton Abramowitz and Irene Stegun, Handbook of Mathematical Functions,
//   Dover Publications, New York [1970]
//   https://en.wikipedia.org/wiki/Abramowitz_and_Stegun

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-numbers.h"
#include "a68g-math.h"

inline REAL_T a68_max (REAL_T x, REAL_T y)
{
  return (x > y ? x : y);
}

inline REAL_T a68_min (REAL_T x, REAL_T y)
{
  return (x < y ? x : y);
}

inline REAL_T a68_sign (REAL_T x)
{
  return (x == 0 ? 0 : (x > 0 ? 1 : -1));
}

inline REAL_T a68_int (REAL_T x)
{
  return (x >= 0 ? (INT_T) x : -(INT_T) - x);
}

inline INT_T a68_round (REAL_T x)
{
  return (INT_T) (x >= 0 ? x + 0.5 : x - 0.5);
}

#define IS_INTEGER(n) (n == a68_int (n))

inline REAL_T a68_abs (REAL_T x)
{
  return (x >= 0 ? x : -x);
}

REAL_T a68_fdiv (REAL_T x, REAL_T y)
{
// This is for generating +-INF.
  return x / y;
}

REAL_T a68_nan (void)
{
  return a68_fdiv (0.0, 0.0);
}

REAL_T a68_posinf (void)
{
  return a68_fdiv (+1.0, 0.0);
}

REAL_T a68_neginf (void)
{
  return a68_fdiv (-1.0, 0.0);
}

// REAL infinity

void genie_infinity_real (NODE_T * p)
{
  PUSH_VALUE (p, a68_posinf (), A68_REAL);
}

// REAL minus infinity

void genie_minus_infinity_real (NODE_T * p)
{
  PUSH_VALUE (p, a68_neginf (), A68_REAL);
}

#if defined (HAVE_ISFINITE)
int a68_finite (REAL_T x)
{
  return isfinite (x);
}
#elif defined (HAVE_FINITE)
int a68_finite (REAL_T x)
{
  return finite (x);
}
#else
#error "cannot define a68_finite"
#endif

#if defined (HAVE_ISNAN)
int a68_isnan (REAL_T x)
{
  return isnan (x);
}
#elif defined (HAVE_IEEE_COMPARISONS)
int a68_isnan (REAL_T x)
{
  int status = (x != x);
  return status;
}
#else
#error "cannot define a68_isnan"
#endif

#if defined (HAVE_ISINF)
int a68_isinf (REAL_T x)
{
  if (isinf (x)) {
    return (x > 0) ? 1 : -1;
  } else {
    return 0;
  }
}
#else
int a68_isinf (REAL_T x)
{
  if (!a68_finite (x) && !a68_isnan (x)) {
    return (x > 0 ? +1 : -1);
  } else {
    return 0;
  }
}
#endif

// INT operators

INT_T a68_add_int (INT_T j, INT_T k)
{
  if (j >= 0) {
    A68_OVERFLOW (A68_MAX_INT - j < k);
  } else {
    A68_OVERFLOW (k < (-A68_MAX_INT) - j);
  }
  return j + k;
}

INT_T a68_sub_int (INT_T j, INT_T k)
{
  return a68_add_int (j, -k);
}

INT_T a68_mul_int (INT_T j, INT_T k)
{
  if (j == 0 || k == 0) {
    return 0;
  } else {
    INT_T u = (j > 0 ? j : -j), v = (k > 0 ? k : -k);
    A68_OVERFLOW (u > A68_MAX_INT / v);
    return j * k;
  }
}

INT_T a68_over_int (INT_T j, INT_T k)
{
  A68_INVALID (k == 0);
  return j / k;
}

INT_T a68_mod_int (INT_T j, INT_T k)
{
  A68_INVALID (k == 0);
  INT_T r = j % k;
  return (r < 0 ? (k > 0 ? r + k : r - k) : r);
}

// OP ** = (INT, INT) INT 

INT_T a68_m_up_n (INT_T m, INT_T n)
{
// Only positive n.
  A68_INVALID (n < 0);
// Special cases.
  if (m == 0 || m == 1) {
    return m;
  } else if (m == -1) {
    return (EVEN (n) ? 1 : -1);
  }
// General case with overflow check.
  UNSIGNED_T bit = 1;
  INT_T M = m, P = 1;
  do {
    if (n & bit) {
      P = a68_mul_int (P, M);
    }
    bit <<= 1;
    if (bit <= n) {
      M = a68_mul_int (M, M);
    }
  } while (bit <= n);
  return P;
}

// OP ** = (REAL, INT) REAL 

REAL_T a68_x_up_n (REAL_T x, INT_T n)
{
// Only positive n.
  if (n < 0) {
    return 1 / a68_x_up_n (x, -n);
  }
// Special cases.
  if (x == 0 || x == 1) {
    return x;
  } else if (x == -1) {
    return (EVEN (n) ? 1 : -1);
  }
// General case.
  UNSIGNED_T bit = 1;
  REAL_T M = x, P = 1;
  do {
    if (n & bit) {
      P *= M;
    }
    bit <<= 1;
    if (bit <= n) {
      M *= M;
    }
  } while (bit <= n);
  A68_OVERFLOW (!finite (P));
  return P;
}

REAL_T a68_div_int (INT_T j, INT_T k)
{
  A68_INVALID (k == 0);
  return (REAL_T) j / (REAL_T) k;
}

// Sqrt (x^2 + y^2) that does not needlessly overflow.

REAL_T a68_hypot (REAL_T x, REAL_T y)
{
  REAL_T xabs = ABS (x), yabs = ABS (y), min, max;
  if (xabs < yabs) {
    min = xabs;
    max = yabs;
  } else {
    min = yabs;
    max = xabs;
  }
  if (min == 0) {
    return max;
  } else {
    REAL_T u = min / max;
    return max * sqrt (1 + u * u);
  }
}

//! @brief Compute Chebyshev series to requested accuracy.

REAL_T a68_chebyshev (REAL_T x, const REAL_T c[], REAL_T acc)
{
// Iteratively compute the recursive Chebyshev series.
// c[1..N] are coefficients, c[0] is N, and acc is relative accuracy.
  acc *= MATH_EPSILON;
  if (acc < c[1]) {
    diagnostic (A68_MATH_WARNING, A68 (f_entry), WARNING_MATH_ACCURACY, NULL);
  }
  INT_T i, N = a68_round (c[0]);
  REAL_T err = 0, z = 2 * x, u = 0, v = 0, w = 0;
  for (i = 1; i <= N; i++) {
    if (err > acc) {
      w = v;
      v = u;
      u = z * v - w + c[i];
    }
    err += a68_abs (c[i]);
  }
  return 0.5 * (u - w);
}

// Compute ln (1 + x) accurately. 
// Some C99 platforms implement this incorrectly.

REAL_T a68_ln1p (REAL_T x)
{
// Based on GNU GSL's gsl_sf_log_1plusx_e.
  A68_INVALID (x <= -1);
  if (a68_abs (x) < pow (DBL_EPSILON, 1 / 6.0)) {
    const REAL_T c1 = -0.5, c2 = 1 / 3.0, c3 = -1 / 4.0, c4 = 1 / 5.0, c5 = -1 / 6.0, c6 = 1 / 7.0, c7 = -1 / 8.0, c8 = 1 / 9.0, c9 = -1 / 10.0;
    const REAL_T t = c5 + x * (c6 + x * (c7 + x * (c8 + x * c9)));
    return x * (1 + x * (c1 + x * (c2 + x * (c3 + x * (c4 + x * t)))));
  } else if (a68_abs (x) < 0.5) {
    REAL_T t = (8 * x + 1) / (x + 2) / 2;
    return x * a68_chebyshev (t, c_ln1p, 0.1);
  } else {
    return ln (1 + x);
  }
}

// Compute ln (x), if possible accurately when x ~ 1.

REAL_T a68_ln (REAL_T x)
{
  A68_INVALID (x <= 0);
#if (A68_LEVEL >= 3)
  if (a68_abs (x - 1) < 0.375) {
// Double precision x-1 mitigates cancellation error.
    return a68_ln1p (DBLEQ (x) - 1.0q);
  } else {
    return ln (x);
  }
#else
  return ln (x);
#endif
}

// PROC (REAL) REAL exp

REAL_T a68_exp (REAL_T x)
{
  A68_INVALID (x < LOG_DBL_MIN || x > LOG_DBL_MAX);
  return exp (x);
}

// OP ** = (REAL, REAL) REAL

REAL_T a68_x_up_y (REAL_T x, REAL_T y)
{
  return a68_exp (y * a68_ln (x));
}

// PROC (REAL) REAL csc

REAL_T a68_csc (REAL_T x)
{
  REAL_T z = sin (x);
  A68_OVERFLOW (z == 0);
  return 1 / z;
}

// PROC (REAL) REAL acsc

REAL_T a68_acsc (REAL_T x)
{
  A68_OVERFLOW (x == 0);
  return asin (1 / x);
}

// PROC (REAL) REAL sec

REAL_T a68_sec (REAL_T x)
{
  REAL_T z = cos (x);
  A68_OVERFLOW (z == 0);
  return 1 / z;
}

// PROC (REAL) REAL asec

REAL_T a68_asec (REAL_T x)
{
  A68_OVERFLOW (x == 0);
  return acos (1 / x);
}

// PROC (REAL) REAL cot

REAL_T a68_cot (REAL_T x)
{
  REAL_T z = sin (x);
  A68_OVERFLOW (z == 0);
  return cos (x) / z;
}

// PROC (REAL) REAL acot

REAL_T a68_acot (REAL_T x)
{
  A68_OVERFLOW (x == 0);
  return atan (1 / x);
}

// PROC atan2 (REAL, REAL) REAL

REAL_T a68_atan2 (REAL_T x, REAL_T y)
{
  if (x == 0) {
    A68_INVALID (y == 0);
    return (y > 0 ? CONST_PI_2 : -CONST_PI_2);
  } else {
    REAL_T z = atan (ABS (y / x));
    if (x < 0) {
      z = CONST_PI - z;
    }
    return (y >= 0 ? z : -z);
  }
}

//! brief PROC (REAL) REAL sindg

REAL_T a68_sindg (REAL_T x)
{
  return sin (x * CONST_PI_OVER_180);
}

//! brief PROC (REAL) REAL cosdg

REAL_T a68_cosdg (REAL_T x)
{
  return cos (x * CONST_PI_OVER_180);
}

//! brief PROC (REAL) REAL tandg

REAL_T a68_tandg (REAL_T x)
{
  return tan (x * CONST_PI_OVER_180);
}

//! brief PROC (REAL) REAL asindg

REAL_T a68_asindg (REAL_T x)
{
  return asin (x) * CONST_180_OVER_PI;
}

//! brief PROC (REAL) REAL acosdg

REAL_T a68_acosdg (REAL_T x)
{
  return acos (x) * CONST_180_OVER_PI;
}

//! brief PROC (REAL) REAL atandg

REAL_T a68_atandg (REAL_T x)
{
  return atan (x) * CONST_180_OVER_PI;
}

// PROC (REAL) REAL cotdg

REAL_T a68_cotdg (REAL_T x)
{
  REAL_T z = a68_sindg (x);
  A68_OVERFLOW (z == 0);
  return a68_cosdg (x) / z;
}

// PROC (REAL) REAL acotdg

REAL_T a68_acotdg (REAL_T z)
{
  A68_OVERFLOW (z == 0);
  return a68_atandg (1 / z);
}

// @brief PROC (REAL) REAL sinpi

REAL_T a68_sinpi (REAL_T x)
{
  x = fmod (x, 2);
  if (x <= -1) {
    x += 2;
  } else if (x > 1) {
    x -= 2;
  }
// x in <-1, 1].
  if (x == 0 || x == 1) {
    return 0;
  } else if (x == 0.5) {
    return 1;
  }
  if (x == -0.5) {
    return -1;
  } else {
    return sin (CONST_PI * x);
  }
}

// @brief PROC (REAL) REAL cospi

REAL_T a68_cospi (REAL_T x)
{
  x = fmod (fabs (x), 2);
// x in [0, 2>.
  if (x == 0.5 || x == 1.5) {
    return 0;
  } else if (x == 0) {
    return 1;
  } else if (x == 1) {
    return -1;
  } else {
    return cos (CONST_PI * x);
  }
}

// @brief PROC (REAL) REAL tanpi

REAL_T a68_tanpi (REAL_T x)
{
  x = fmod (x, 1);
  if (x <= -0.5) {
    x += 1;
  } else if (x > 0.5) {
    x -= 1;
  }
// x in <-1/2, 1/2].
  A68_OVERFLOW (x == 0.5);
  if (x == -0.25) {
    return -1;
  } else if (x == 0) {
    return 0;
  } else if (x == 0.25) {
    return 1;
  } else {
    return a68_sinpi (x) / a68_cospi (x);
  }
}

// @brief PROC (REAL) REAL cotpi

REAL_T a68_cotpi (REAL_T x)
{
  x = fmod (x, 1);
  if (x <= -0.5) {
    x += 1;
  } else if (x > 0.5) {
    x -= 1;
  }
// x in <-1/2, 1/2].
  A68_OVERFLOW (x == 0);
  if (x == -0.25) {
    return -1;
  } else if (x == 0.25) {
    return 1;
  } else if (x == 0.5) {
    return 0;
  } else {
    return a68_cospi (x) / a68_sinpi (x);
  }
}

// @brief PROC (REAL) REAL asinh

REAL_T a68_asinh (REAL_T x)
{
  REAL_T a = ABS (x), s = (x < 0 ? -1.0 : 1);
  if (a > 1 / sqrt (DBL_EPSILON)) {
    return (s * (a68_ln (a) + a68_ln (2)));
  } else if (a > 2) {
    return (s * a68_ln (2 * a + 1 / (a + sqrt (a * a + 1))));
  } else if (a > sqrt (DBL_EPSILON)) {
    REAL_T a2 = a * a;
    return (s * a68_ln1p (a + a2 / (1 + sqrt (1 + a2))));
  } else {
    return (x);
  }
}

// @brief PROC (REAL) REAL acosh

REAL_T a68_acosh (REAL_T x)
{
  if (x > 1 / sqrt (DBL_EPSILON)) {
    return (a68_ln (x) + a68_ln (2));
  } else if (x > 2) {
    return (a68_ln (2 * x - 1 / (sqrt (x * x - 1) + x)));
  } else if (x > 1) {
    REAL_T t = x - 1;
    return (a68_ln1p (t + sqrt (2 * t + t * t)));
  } else if (x == 1) {
    return (0);
  } else {
    A68_INVALID (A68_TRUE);
  }
}

// @brief PROC (REAL) REAL atanh

REAL_T a68_atanh (REAL_T x)
{
  REAL_T a = ABS (x);
  A68_INVALID (a >= 1);
  REAL_T s = (x < 0 ? -1 : 1);
  if (a >= 0.5) {
    return (s * 0.5 * a68_ln1p (2 * a / (1 - a)));
  } else if (a > DBL_EPSILON) {
    return (s * 0.5 * a68_ln1p (2 * a + 2 * a * a / (1 - a)));
  } else {
    return (x);
  }
}

//! @brief Inverse complementary error function.

REAL_T a68_inverfc (REAL_T y)
{
  A68_INVALID (y < 0 || y > 2);
  if (y == 0) {
    return DBL_MAX;
  } else if (y == 1) {
    return 0;
  } else if (y == 2) {
    return -DBL_MAX;
  } else {
// Next is based on code that originally contained following statement:
//   Copyright (c) 1996 Takuya Ooura. You may use, copy, modify this 
//   code for any purpose and without fee.
    REAL_T s, t, u, v, x, z;
    z = (y <= 1 ? y : 2 - y);
    v = c_inverfc[0] - a68_ln (z);
    u = sqrt (v);
    s = (a68_ln (u) + c_inverfc[1]) / v;
    t = 1 / (u + c_inverfc[2]);
    x = u * (1 - s * (s * c_inverfc[3] + 0.5)) - ((((c_inverfc[4] * t + c_inverfc[5]) * t + c_inverfc[6]) * t + c_inverfc[7]) * t + c_inverfc[8]) * t;
    t = c_inverfc[9] / (x + c_inverfc[9]);
    u = t - 0.5;
    s = (((((((((c_inverfc[10] * u + c_inverfc[11]) * u - c_inverfc[12]) * u - c_inverfc[13]) * u + c_inverfc[14]) * u + c_inverfc[15]) * u - c_inverfc[16]) * u - c_inverfc[17]) * u + c_inverfc[18]) * u + c_inverfc[19]) * u + c_inverfc[20];
    s = ((((((((((((s * u - c_inverfc[21]) * u - c_inverfc[22]) * u + c_inverfc[23]) * u + c_inverfc[24]) * u + c_inverfc[25]) * u + c_inverfc[26]) * u + c_inverfc[27]) * u + c_inverfc[28]) * u + c_inverfc[29]) * u + c_inverfc[30]) * u + c_inverfc[31]) * u + c_inverfc[32]) * t - z * a68_exp (x * x - c_inverfc[33]);
    x += s * (x * s + 1);
    return (y <= 1 ? x : -x);
  }
}

//! @brief Inverse error function.

REAL_T a68_inverf (REAL_T y)
{
  return a68_inverfc (1 - y);
}

//! @brief PROC (REAL, REAL) REAL ln beta

REAL_T a68_ln_beta (REAL_T a, REAL_T b)
{
  return lgamma (a) + lgamma (b) - lgamma (a + b);
}

//! @brief PROC (REAL, REAL) REAL beta

REAL_T a68_beta (REAL_T a, REAL_T b)
{
  return a68_exp (a68_ln_beta (a, b));
}

//! brief PROC (INT) REAL fact

REAL_T a68_fact (INT_T n)
{
  A68_INVALID (n < 0 || n > A68_MAX_FAC);
  return factable[n];
}

//! brief PROC (INT) REAL ln fact

REAL_T a68_ln_fact (INT_T n)
{
  A68_INVALID (n < 0);
  if (n <= A68_MAX_FAC) {
    return ln_factable[n];
  } else {
    return lgamma (n + 1);
  }
}

//! @brief PROC choose = (INT n, m) REAL

REAL_T a68_choose (INT_T n, INT_T m)
{
  A68_INVALID (n < m);
  return factable[n] / (factable[m] * factable[n - m]);
}

//! @brief PROC ln choose = (INT n, m) REAL

REAL_T a68_ln_choose (INT_T n, INT_T m)
{
  A68_INVALID (n < m);
  return a68_ln_fact (n) - (a68_ln_fact (m) + a68_ln_fact (n - m));
}

REAL_T a68_beta_inc (REAL_T s, REAL_T t, REAL_T x)
{
// Incomplete beta function I{x}(s, t).
// Continued fraction, see dlmf.nist.gov/8.17; Lentz's algorithm.
  if (x < 0 || x > 1) {
    errno = ERANGE;
    return -1;
  } else {
    const INT_T lim = 16 * sizeof (REAL_T);
    BOOL_T cont = A68_TRUE;
// Rapid convergence when x <= (s+1)/(s+t+2) or else recursion.
    if (x > (s + 1) / (s + t + 2)) {
// B{x}(s, t) = 1 - B{1-x}(t, s)
      return 1 - a68_beta_inc (s, t, 1 - x);
    }
// Lentz's algorithm for continued fraction.
    REAL_T W = 1, F = 1, c = 1, d = 0;
    INT_T N, m;
    for (N = 0, m = 0; cont && N < lim; N++) {
      REAL_T T;
      if (N == 0) {
        T = 1;
      } else if (N % 2 == 0) {
// d{2m} := x m(t-m)/((s+2m-1)(s+2m))
        T = x * m * (t - m) / (s + 2 * m - 1) / (s + 2 * m);
      } else {
// d{2m+1} := -x (s+m)(s+t+m)/((s+2m+1)(s+2m))
        T = -x * (s + m) * (s + t + m) / (s + 2 * m + 1) / (s + 2 * m);
        m++;
      }
      d = 1 / (T * d + 1);
      c = T / c + 1;
      F *= c * d;
      if (F == W) {
        cont = A68_FALSE;
      } else {
        W = F;
      }
    }
// I{x}(s,t)=x^s(1-x)^t / s / B(s,t) F
    REAL_T beta = a68_exp (lgamma (s) + lgamma (t) - lgamma (s + t));
    return a68_x_up_y (x, s) * a68_x_up_y (1 - x, t) / s / beta * (F - 1);
  }
}
