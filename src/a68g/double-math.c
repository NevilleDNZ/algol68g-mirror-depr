//! @file double-math.c
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
//! LONG REAL, LONG COMPLEX routines. 

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

#if (A68_LEVEL >= 3)

DOUBLE_T a68_beta_inc_double (DOUBLE_T s, DOUBLE_T t, DOUBLE_T x)
{
// Incomplete beta function I{x}(s, t).
// Continued fraction, see dlmf.nist.gov/8.17; Lentz's algorithm.
  if (x < 0.0q || x > 1.0q) {
    errno = ERANGE;
    return -1.0q;
  } else {
    const INT_T lim = 16 * sizeof (DOUBLE_T);
// Rapid convergence when x <= (s+1)/(s+t+2) or else recursion.
    if (x > (s + 1.0q) / (s + t + 2.0q)) {
// B{x}(s, t) = 1 - B{1-x}(t, s)
      return 1.0q - a68_beta_inc_double (s, t, 1.0q - x);
    }
// Lentz's algorithm for continued fraction.
    DOUBLE_T W = 1.0q, F = 1.0q, c = 1.0q, d = 0.0q;
    BOOL_T cont = A68_TRUE;
    for (int N = 0, m = 0; cont && N < lim; N++) {
      DOUBLE_T T;
      if (N == 0) {
        T = 1.0q;
      } else if (N % 2 == 0) {
// d{2m} := x m(t-m)/((s+2m-1)(s+2m))
        T = x * m * (t - m) / (s + 2.0q * m - 1.0q) / (s + 2.0q * m);
      } else {
// d{2m+1} := -x (s+m)(s+t+m)/((s+2m+1)(s+2m))
        T = -x * (s + m) * (s + t + m) / (s + 2.0q * m + 1.0q) / (s + 2.0q * m);
        m++;
      }
      d = 1.0q / (T * d + 1.0q);
      c = T / c + 1.0q;
      F *= c * d;
      if (F == W) {
        cont = A68_FALSE;
      } else {
        W = F;
      }
    }
// I{x}(s,t)=x^s(1-x)^t / s / B(s,t) F
    DOUBLE_T beta = exp_double (lgamma_double (s) + lgamma_double (t) - lgamma_double (s + t));
    return pow_double (x, s) * pow_double (1.0q - x, t) / s / beta * (F - 1.0q);
  }
}

//! @brief PROC (LONG REAL) LONG REAL csc

DOUBLE_T csc_double (DOUBLE_T x)
{
  DOUBLE_T z = sin_double (x);
  A68_OVERFLOW (z == 0.0q);
  return 1.0q / z;
}

//! @brief PROC (LONG REAL) LONG REAL acsc

DOUBLE_T acsc_double (DOUBLE_T x)
{
  A68_OVERFLOW (x == 0.0q);
  return asin_double (1.0q / x);
}

//! @brief PROC (LONG REAL) LONG REAL sec

DOUBLE_T sec_double (DOUBLE_T x)
{
  DOUBLE_T z = cos_double (x);
  A68_OVERFLOW (z == 0.0q);
  return 1.0q / z;
}

//! @brief PROC (LONG REAL) LONG REAL asec

DOUBLE_T asec_double (DOUBLE_T x)
{
  A68_OVERFLOW (x == 0.0q);
  return acos_double (1.0q / x);
}

//! @brief PROC (LONG REAL) LONG REAL cot

DOUBLE_T cot_double (DOUBLE_T x)
{
  DOUBLE_T z = sin_double (x);
  A68_OVERFLOW (z == 0.0q);
  return cos_double (x) / z;
}

//! @brief PROC (LONG REAL) LONG REAL acot

DOUBLE_T acot_double (DOUBLE_T x)
{
  A68_OVERFLOW (x == 0.0q);
  return atan_double (1 / x);
}

//! brief PROC (LONG REAL) LONG REAL sindg

DOUBLE_T sindg_double (DOUBLE_T x)
{
  return sin_double (x * CONST_PI_OVER_180_Q);
}

//! brief PROC (LONG REAL) LONG REAL cosdg

DOUBLE_T cosdg_double (DOUBLE_T x)
{
  return cos_double (x * CONST_PI_OVER_180_Q);
}

//! brief PROC (LONG REAL) LONG REAL tandg

DOUBLE_T tandg_double (DOUBLE_T x)
{
  return tan_double (x * CONST_PI_OVER_180_Q);
}

//! brief PROC (LONG REAL) LONG REAL asindg

DOUBLE_T asindg_double (DOUBLE_T x)
{
  return asin_double (x) * CONST_180_OVER_PI_Q;
}

//! brief PROC (LONG REAL) LONG REAL acosdg

DOUBLE_T acosdg_double (DOUBLE_T x)
{
  return acos_double (x) * CONST_180_OVER_PI_Q;
}

//! brief PROC (LONG REAL) LONG REAL atandg

DOUBLE_T atandg_double (DOUBLE_T x)
{
  return atan_double (x) * CONST_180_OVER_PI_Q;
}

// PROC (LONG REAL) LONG REAL cotdg

DOUBLE_T cotdg_double (DOUBLE_T x)
{
  DOUBLE_T z = a68_sindg (x);
  A68_OVERFLOW (z == 0);
  return a68_cosdg (x) / z;
}

// PROC (LONG REAL) LONG REAL acotdg

DOUBLE_T acotdg_double (DOUBLE_T z)
{
  A68_OVERFLOW (z == 0);
  return a68_atandg (1 / z);
}

// @brief PROC (LONG REAL) LONG REAL sinpi

DOUBLE_T sinpi_double (DOUBLE_T x)
{
  x = fmod_double (x, 2.0q);
  if (x <= -1.0q) {
    x += 2.0q;
  } else if (x > 1.0q) {
    x -= 2.0q;
  }
// x in <-1, 1].
  if (x == 0.0q || x == 1.0q) {
    return 0.0q;
  } else if (x == 0.5q) {
    return 1.0q;
  }
  if (x == -0.5q) {
    return -1.0q;
  } else {
    return sin_double (CONST_PI_Q * x);
  }
}

// @brief PROC (LONG REAL) LONG REAL cospi

DOUBLE_T cospi_double (DOUBLE_T x)
{
  x = fmod_double (fabs_double (x), 2.0q);
// x in [0, 2>.
  if (x == 0.5q || x == 1.5q) {
    return 0.0q;
  } else if (x == 0.0q) {
    return 1.0q;
  } else if (x == 1.0q) {
    return -1.0q;
  } else {
    return cos_double (CONST_PI_Q * x);
  }
}

// @brief PROC (LONG REAL) LONG REAL tanpi

DOUBLE_T tanpi_double (DOUBLE_T x)
{
  x = fmod_double (x, 1.0q);
  if (x <= -0.5q) {
    x += 1.0q;
  } else if (x > 0.5q) {
    x -= 1.0q;
  }
// x in <-1/2, 1/2].
  A68_OVERFLOW (x == 0.5q);
  if (x == -0.25q) {
    return -1.0q;
  } else if (x == 0) {
    return 0.0q;
  } else if (x == 0.25q) {
    return 1.0q;
  } else {
    return sinpi_double (x) / cospi_double (x);
  }
}

// @brief PROC (LONG REAL) LONG REAL cotpi

DOUBLE_T cotpi_double (DOUBLE_T x)
{
  x = fmod_double (x, 1.0q);
  if (x <= -0.5q) {
    x += 1.0q;
  } else if (x > 0.5q) {
    x -= 1.0q;
  }
// x in <-1/2, 1/2].
  A68_OVERFLOW (x == 0.0q);
  if (x == -0.25q) {
    return -1.0q;
  } else if (x == 0.25q) {
    return 1.0q;
  } else if (x == 0.5q) {
    return 0.0q;
  } else {
    return cospi_double (x) / sinpi_double (x);
  }
}

#endif
