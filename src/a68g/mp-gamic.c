//! @file mp-gamic.c
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
//! [LONG] LONG REAL generalised incomplete gamma function.

// Generalised incomplete gamma code in this file was downloaded from 
//   [http://helios.mi.parisdescartes.fr/~rabergel/]
// and adapted for Algol 68 Genie.
//
// Reference:
//   Rémy Abergel, Lionel Moisan. Fast and accurate evaluation of a
//   generalized incomplete gamma function. 2019. hal-01329669v2
//
// Original source code copyright and license:
//
// DELTAGAMMAINC Fast and Accurate Evaluation of a Generalized Incomplete Gamma
// Function. Copyright (C) 2016 Remy Abergel (remy.abergel AT gmail.com), Lionel
// Moisan (Lionel.Moisan AT parisdescartes.fr).
//
// This file is a part of the DELTAGAMMAINC software, dedicated to the
// computation of a generalized incomplete gammafunction. See the Companion paper
// for a complete description of the algorithm.
//
// ``Fast and accurate evaluation of a generalized incomplete gamma function''
// (Rémy Abergel, Lionel Moisan), preprint MAP5 nº2016-14, revision 1.
//
// This program is free software: you can redistribute it and/or modify it under
// the terms of the GNU General Public License as published by the Free Software
// Foundation, either version 3 of the License, or (at your option) any later
// version.
//
// This program is distributed in the hope that it will be useful, but WITHOUT
// ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
// FOR A PARTICULAR PURPOSE.  See the GNU General Public License for more
// details.
//
// You should have received a copy of the GNU General Public License along with
// this program.  If not, see [http://www.gnu.org/licenses/].

// References
//
//   R. Abergel and L. Moisan. 2016. Fast and accurate evaluation of a
//   generalized incomplete gamma function, preprint MAP5 nº2016-14, revision 1
//
//   Rémy Abergel, Lionel Moisan. Fast and accurate evaluation of a
//   generalized incomplete gamma function. 2019. hal-01329669v2
//
//   F. W. J. Olver, D. W. Lozier, R. F. Boisvert, and C. W. Clark
//   (Eds.). 2010. NIST Handbook of Mathematical Functions. Cambridge University
//   Press. (see online version at [http://dlmf.nist.gov/])
//
//   W. H. Press, S. A. Teukolsky, W. T. Vetterling, and
//   B. P. Flannery. 1992. Numerical recipes in C: the art of scientific
//   computing (2nd ed.).
//
//   G. R. Pugh, 2004. An analysis of the Lanczos Gamma approximation (phd
//   thesis)

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-mp.h"
#include "a68g-lib.h"

// Processing time of Abergel's algorithms rises steeply with precision.
#define MAX_PRECISION (LONG_LONG_MP_DIGITS + LONG_MP_DIGITS)
#define GAM_DIGITS(digs) FUN_DIGITS (digs)

#define ITMAX(p, digits) lit_mp (p, 1000000, 0, digits);
#define DPMIN(p, digits) lit_mp (p, 1, 10 - MAX_MP_EXPONENT, digits)
#define EPS(p, digits) lit_mp (p, 1, 1 - digits, digits)
#define NITERMAX_ROMBERG 16 // Maximum allowed number of Romberg iterations
#define TOL_ROMBERG(p, digits) lit_mp (p, MP_RADIX / 10, -1, digits)
#define TOL_DIFF(p, digits) lit_mp (p, MP_RADIX / 5, -1, digits)

//! @brief compute G(p,x) in the domain x <= p >= 0 using a continued fraction

MP_T *G_cfrac_lower_mp (NODE_T *q, MP_T * Gcfrac, MP_T *p, MP_T *x, int digs)
{
  if (IS_ZERO_MP (x)) {
    SET_MP_ZERO (Gcfrac, digs);
    return Gcfrac;
  }
  ADDR_T pop_sp = A68_SP;
  MP_T *c = nil_mp (q, digs);
  MP_T *d = nil_mp (q, digs);
  MP_T *del = nil_mp (q, digs);
  MP_T *f = nil_mp (q, digs);
// Evaluate the continued fraction using Modified Lentz's method. However,
// as detailed in the paper, perform manually the first pass (n=1), of the
// initial Modified Lentz's method.
// an = 1; bn = p; f = an / bn; c = an / DPMIN; d = 1 / bn; n = 2;
  MP_T *an = lit_mp (q, 1, 0, digs);
  MP_T *bn = nil_mp (q, digs);
  MP_T *trm = nil_mp (q, digs);
  MP_T *dpmin = DPMIN (q, digs);
  MP_T *eps = EPS (q, digs);
  MP_T *itmax = ITMAX (q, digs);
  (void) move_mp (bn, p, digs);
  (void) div_mp (q, f, an, bn, digs);
  (void) div_mp (q, c, an, dpmin, digs);
  (void) rec_mp (q, d, bn, digs);
  MP_T *n = lit_mp (q, 2, 0, digs);
  MP_T *k = nil_mp (q, digs);
  MP_T *two = lit_mp (q, 2, 0, digs);
  BOOL_T odd = A68_FALSE, cont = A68_TRUE;
  while (cont) {
    A68_BOOL ge, lt;
//  k = n / 2;
    (void) over_mp (q, k, n, two, digs);    
//  an = (n & 1 ? k : -(p - 1 + k)) * x;
    if (odd) {
      (void) move_mp (an, k, digs);
      odd = A68_FALSE;
    } else {
      (void) minus_one_mp (q, trm, p, digs);
      (void) add_mp (q, trm, trm, k, digs);
      (void) minus_mp (q, an, trm, digs);
      odd = A68_TRUE;
    }
    (void) mul_mp (q, an, an, x, digs);
//  bn++;
    (void) plus_one_mp (q, bn, bn, digs);
//  d = an * d + bn;
    (void) mul_mp (q, trm, an, d, digs);
    (void) add_mp (q, d, trm, bn, digs);
//  if (d == 0) { d = DPMIN; }
    if (IS_ZERO_MP (d)) {
      (void) move_mp (d, dpmin, digs);
    }
//  c = bn + an / c; mind possible overflow.
    (void) div_mp (q, trm, an, c, digs);
    (void) add_mp (q, c, bn, trm, digs);
//  if (c == 0) { c = DPMIN; }
    if (IS_ZERO_MP (c)) {
      (void) move_mp (c, dpmin, digs);
    }
//  d = 1 / d;
    (void) rec_mp (q, d, d, digs);
//  del = d * c;
    (void) mul_mp (q, del, d, c, digs);
//  f *= del;
    (void) mul_mp (q, f, f, del, digs);
//  n++;
    (void) plus_one_mp (q, n, n, digs);
//  while ((fabs_double (del - 1) >= EPS) && (n < ITMAX));
    (void) minus_one_mp (q, trm, del, digs);
    (void) abs_mp (q, trm, trm, digs);
    (void) ge_mp (q, &ge, trm, eps, digs);
    (void) lt_mp (q, &lt, n, itmax, digs);
    cont = VALUE (&ge) && VALUE (&lt);
  }
  (void) move_mp (Gcfrac, f, digs);
  A68_SP = pop_sp;
  return Gcfrac;
}

//! @brief compute the G-function in the domain x > p using a
// continued fraction.
//
// 0 < p < x, or x = +infinity

MP_T *G_cfrac_upper_mp (NODE_T *q, MP_T * Gcfrac, MP_T *p, MP_T *x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if (PLUS_INF_MP (x)) {
    SET_MP_ZERO (Gcfrac, digs);
    return Gcfrac;
  }
  MP_T *c = nil_mp (q, digs);
  MP_T *d = nil_mp (q, digs);
  MP_T *del = nil_mp (q, digs);
  MP_T *f = nil_mp (q, digs);
  MP_T *trm = nil_mp (q, digs);
  MP_T *dpmin = DPMIN (q, digs);
  MP_T *eps = EPS (q, digs);
  MP_T *itmax = ITMAX (q, digs);
  MP_T *n = lit_mp (q, 2, 0, digs);
  MP_T *i = nil_mp (q, digs);
  MP_T *two = lit_mp (q, 2, 0, digs);
// an = 1;
  MP_T *an = lit_mp (q, 1, 0, digs);
// bn = x + 1 - p;
  MP_T *bn = lit_mp (q, 1, 0, digs);
  (void) add_mp (q, bn, x, bn, digs);
  (void) sub_mp (q, bn, bn, p, digs);
  BOOL_T t = !IS_ZERO_MP (bn);
// Evaluate the continued fraction using Modified Lentz's method. However,
// as detailed in the paper, perform manually the first pass (n=1), of the
// initial Modified Lentz's method.
  if (t) {
// b{1} is non-zero
// f = an / bn;
    (void) div_mp (q, f, an, bn, digs);
// c = an / DPMIN;
    (void) div_mp (q, c, an, dpmin, digs);
// d = 1 / bn;
    (void) rec_mp (q, d, bn, digs);
// n = 2;
    set_mp (n, 2, 0, digs);
  } else {
// b{1}=0 but b{2} is non-zero, compute Mcfrac = a{1}/f with f = a{2}/(b{2}+) a{3}/(b{3}+) ...
//  an = -(1 - p);
    (void) minus_one_mp (q, an, p, digs);
//  bn = x + 3 - p;
    (void) set_mp (bn, 3, 0, digs);
    (void) add_mp (q, bn, x, bn, digs);
    (void) sub_mp (q, bn, bn, p, digs);
//  f = an / bn;
    (void) div_mp (q, f, an, bn, digs);
//  c = an / DPMIN;
    (void) div_mp (q, c, an, dpmin, digs);
//  d = 1 / bn;
    (void) rec_mp (q, d, bn, digs);
//  n = 3;
    set_mp (n, 3, 0, digs);
  }
//  i = n - 1;
  minus_one_mp (q, i, n, digs); 
  BOOL_T cont = A68_TRUE;  
  while (cont) {
    A68_BOOL ge, lt;
//  an = -i * (i - p);
    (void) sub_mp (q, trm, p, i, digs);
    (void) mul_mp (q, an, i, trm, digs);
//  bn += 2;
    (void) add_mp (q, bn, bn, two, digs);
//  d = an * d + bn;
    (void) mul_mp (q, trm, an, d, digs);
    (void) add_mp (q, d, trm, bn, digs);
//  if (d == 0) { d = DPMIN; }
    if (IS_ZERO_MP (d)) {
      (void) move_mp (d, dpmin, digs);
    }
//  c = bn + an / c; mind possible overflow.
    (void) div_mp (q, trm, an, c, digs);
    (void) add_mp (q, c, bn, trm, digs);
//  if (c == 0) { c = DPMIN; }
    if (IS_ZERO_MP (c)) {
      (void) move_mp (c, dpmin, digs);
    }
//  d = 1 / d;
    (void) rec_mp (q, d, d, digs);
//  del = d * c;
    (void) mul_mp (q, del, d, c, digs);
//  f *= del;
    (void) mul_mp (q, f, f, del, digs);
//  i++;
    (void) plus_one_mp (q, i, i, digs);
//  n++;
    (void) plus_one_mp (q, n, n, digs);
//  while ((fabs_double (del - 1) >= EPS) && (n < ITMAX));
    (void) minus_one_mp (q, trm, del, digs);
    (void) abs_mp (q, trm, trm, digs);
    (void) ge_mp (q, &ge, trm, eps, digs);
    (void) lt_mp (q, &lt, n, itmax, digs);
    cont = VALUE (&ge) && VALUE (&lt);
  }
  A68_SP = pop_sp;
// *Gcfrac = t ? f : 1 / f;
  if (t) {
    (void) move_mp (Gcfrac, f, digs);
  } else {
    (void) rec_mp (q, Gcfrac, f, digs);
  }
  return Gcfrac;
}

//! @brief compute the G-function in the domain x < 0 and |x| < max (1,p-1)
// using a recursive integration by parts relation.
// This function cannot be used when mu > 0.
//
// p > 0, integer; x < 0, |x| < max (1,p-1)

MP_T *G_ibp_mp (NODE_T *q, MP_T * Gibp, MP_T *p, MP_T *x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *trm = nil_mp (q, digs), *trn = nil_mp (q, digs);
  MP_T *eps = EPS (q, digs);
// t = fabs_double (x);
  MP_T *t = nil_mp (q, digs);
  (void) abs_mp (q, x, x, digs);
// tt = 1 / (t * t);
  MP_T *tt = nil_mp (q, digs);
  (void) mul_mp (q, tt, t, t, digs);
  (void) rec_mp (q, tt, tt, digs);
// odd = (INT_T) (p) % 2 != 0;
  MP_T *two = lit_mp (q, 2, 0, digs);
  (void) trunc_mp (q, trm, p, digs);
  (void) mod_mp (q, trm, trm, two, digs);
  BOOL_T odd = !IS_ZERO_MP (trm);
// c = 1 / t;
  MP_T *c = nil_mp (q, digs);
  (void) rec_mp (q, c, t, digs);
// d = (p - 1);
  MP_T *d = nil_mp (q, digs);
  (void) minus_one_mp (q, d, p, digs);
// s = c * (t - d);
  MP_T *s = nil_mp (q, digs);
  (void) sub_mp (q, trm, t, d, digs);
  (void) mul_mp (q, s, c, trm, digs);
// l = 0;
  MP_T *l = nil_mp (q, digs);
//
  BOOL_T cont = A68_TRUE, stop;
  MP_T *del = nil_mp (q, digs);
  while (cont) {
//  c *= d * (d - 1) * tt;
    (void) minus_one_mp (q, trm, d, digs);
    (void) mul_mp (q, trm, d, trm, digs);
    (void) mul_mp (q, trm, trm, tt, digs);
    (void) mul_mp (q, c, c, trm, digs);
//  d -= 2;
    (void) sub_mp (q, d, d, two, digs);
//  del = c * (t - d);
    (void) sub_mp (q, trm, t, d, digs);
    (void) mul_mp (q, del, c, trm, digs);
//  s += del;
    (void) add_mp (q, s, s, del, digs);
//  l++;
    (void) plus_one_mp (q, l, l, digs);
//  stop = fabs_double (del) < fabs_double (s) * EPS;
    (void) abs_mp (q, trm, del, digs);
    (void) abs_mp (q, trn, s, digs);
    (void) mul_mp (q, trn, trn, eps, digs);
    A68_BOOL lt;
    (void) lt_mp (q, &lt, trm, trn, digs);
    stop = VALUE (&lt);
//while ((l < floor_double ((p - 2) / 2)) && !stop);
    (void) sub_mp (q, trm, p, two, digs);
    (void) half_mp (q, trm, trm, digs);
    (void) floor_mp (q, trm, trm, digs);
    (void) lt_mp (q, &lt, l, trm, digs);
    cont = VALUE (&lt) && !stop;
  }
  if (odd && !stop) {
//   s += d * c / t;
    (void) div_mp (q, trm, c, t, digs);
    (void) mul_mp (q, trm, d, trm, digs);
    (void) add_mp (q, s, s, trm, digs);
  }
// Gibp = ((odd ? -1 : 1) * exp_double (-t + lgammaq (p) - (p - 1) * log_double (t)) + s) / t;
  (void) ln_mp (q, trn, t, digs);
  (void) minus_one_mp (q, trm, p, digs);
  (void) mul_mp (q, trm, trm, trn, digs);
  (void) lngamma_mp (q, trn, p, digs);
  (void) sub_mp (q, trm, trn, trm, digs);
  (void) sub_mp (q, trm, trm, t, digs);
  (void) exp_mp (q, Gibp, trm, digs);
  if (odd) {
    (void) minus_mp (q, Gibp, Gibp, digs);
  }
  (void) add_mp (q, Gibp, Gibp, s, digs);
  (void) div_mp (q, Gibp, Gibp, t, digs);
  A68_SP = pop_sp;
  return Gibp;
}

MP_T *plim_mp (NODE_T *p, MP_T *z, MP_T *x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (x, 1) > 0) {
    (void) move_mp (z, x, digs);
  } else {
    MP_T *five = lit_mp (p, 5, 0, digs);
    MP_T *nine = lit_mp (p, -9, 0, digs);
    A68_BOOL ge;
    (void) ge_mp (p, &ge, x, nine, digs);
    if (VALUE (&ge)) {
      SET_MP_ZERO (z, digs);
    } else {
      (void) minus_mp (p, z, x, digs);
      (void) sqrt_mp (p, z, z, digs);
      (void) mul_mp (p, z, five, z, digs);
      (void) sub_mp (p, z, z, five, digs);
    }
  }
  A68_SP = pop_sp;
  return z;
}

//! @brief compute G : (p,x) --> R defined as follows
//
// if x <= p:
//   G(p,x) = exp (x-p*ln (|x|)) * integral of s^{p-1} * exp (-sign (x)*s) ds from s = 0 to |x|
// otherwise:
//   G(p,x) = exp (x-p*ln (|x|)) * integral of s^{p-1} * exp (-s) ds from s = x to infinity
//
//   p > 0; x is a real number or +infinity.

void G_func_mp (NODE_T *q, MP_T * G, MP_T *p, MP_T *x, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *pl = nil_mp (q, digs);
  A68_BOOL ge;
  (void) plim_mp (q, pl, x, digs);
  (void) ge_mp (q, &ge, p, pl, digs);
  if (VALUE (&ge)) {
    G_cfrac_lower_mp (q, G, p, x, digs);
  } else if (MP_DIGIT (x, 1) < 0) {
    G_ibp_mp (q, G, p, x, digs);
  } else {
    G_cfrac_upper_mp (q, G, p, x, digs);
  }
  A68_SP = pop_sp;
}

//! @brief compute I_{x,y}^{mu,p} using a Romberg approximation.
// Compute rho and sigma so I_{x,y}^{mu,p} = rho * exp (sigma)

//! @brief iteration of the Romberg approximation of I_{x,y}^{mu,p}

#define ROMBERG_N (((NITERMAX_ROMBERG + 1) * (NITERMAX_ROMBERG + 2)) / 2)

static inline int IX (int n, int digs) {
  int offset = n * SIZE_MP (digs);
  return offset;
}

void mp_romberg_iterations 
  (NODE_T *q, MP_T *R, MP_T *sigma, INT_T n, MP_T *x, MP_T *y, MP_T *mu, MP_T *p, MP_T *h, MP_T *pow2, int digs)
{
  MP_T *trm = nil_mp (q, digs), *trn = nil_mp (q, digs);
  MP_T *sum = nil_mp (q, digs), *xx = nil_mp (q, digs);
  INT_T adr0_prev = ((n - 1) * n) / 2;
  INT_T adr0 = (n * (n + 1)) / 2;
  MP_T *j = lit_mp (q, 1, 0, digs);
  A68_BOOL le;
  VALUE (&le) = A68_TRUE;
  while (VALUE (&le)) {
//  xx = x + ((y - x) * (2 * j - 1)) / (2 * pow2);
    (void) add_mp (q, trm, j, j, digs);
    (void) minus_one_mp (q, trm, trm, digs);
    (void) sub_mp (q, trn, y, x, digs);
    (void) mul_mp (q, trm, trm, trn, digs);
    (void) div_mp (q, trm, trm, pow2, digs);
    (void) half_mp (q, trm, trm, digs);
    (void) add_mp (q, xx, x, trm, digs);
//  sum += exp (-mu * xx + (p - 1) * a68_ln (xx) - sigma);
    (void) ln_mp (q, trn, xx, digs);
    (void) minus_one_mp (q, trm, p, digs);
    (void) mul_mp (q, trm, trm, trn, digs);
    (void) mul_mp (q, trn, mu, xx, digs);
    (void) sub_mp (q, trm, trm, trn, digs);
    (void) sub_mp (q, trm, trm, sigma, digs);
    (void) exp_mp (q, trm, trm, digs);
    (void) add_mp (q, sum, sum, trm, digs);
// j++;
    (void) plus_one_mp (q, j, j, digs);
    (void) le_mp (q, &le, j, pow2, digs);
  }
// R[adr0] = 0.5 * R[adr0_prev] + h * sum;
  (void) half_mp (q, trm, &R[IX (adr0_prev, digs)], digs);
  (void) mul_mp (q, trn, h, sum, digs);
  (void) add_mp (q, &R[IX (adr0, digs)], trm, trn, digs);
// REAL_T pow4 = 4;
  MP_T *pow4 = lit_mp (q, 4, 0, digs);
  for (unt m = 1; m <= n; m++) {
//  R[adr0 + m] = (pow4 * R[adr0 + (m - 1)] - R[adr0_prev + (m - 1)]) / (pow4 - 1);
    (void) mul_mp (q, trm, pow4, &R[IX (adr0 + m - 1, digs)], digs);
    (void) sub_mp (q, trm, trm, &R[IX (adr0_prev + m - 1, digs)], digs);
    (void) minus_one_mp (q, trn, pow4, digs);
    (void) div_mp (q, &R[IX (adr0 + m, digs)], trm, trn, digs);
//  pow4 *= 4;
    (void) add_mp (q, trm, pow4, pow4, digs);
    (void) add_mp (q, pow4, trm, trm, digs);
  }
}

void mp_romberg_estimate (NODE_T *q, MP_T * rho, MP_T * sigma, MP_T *x, MP_T *y, MP_T *mu, MP_T *p, int digs)
{
  ADDR_T pop_sp = A68_SP;
  MP_T *R = (MP_T *) get_heap_space (ROMBERG_N * SIZE_MP (digs));
// Initialization (n=1)
  MP_T *trm = nil_mp (q, digs), *trn = nil_mp (q, digs);
// sigma = -mu * y + (p - 1) * ln (y);
  (void) ln_mp (q, trn, y, digs);
  (void) minus_one_mp (q, trm, p, digs);
  (void) mul_mp (q, trm, trm, trn, digs);
  (void) mul_mp (q, trn, mu, y, digs);
  (void) sub_mp (q, sigma, trm, trn, digs);
// R[0] = 0.5 * (y - x) * (exp (-mu * x + (p - 1) * ln (x) - (*sigma)) + 1);
  (void) ln_mp (q, trn, x, digs);
  (void) minus_one_mp (q, trm, p, digs);
  (void) mul_mp (q, trm, trm, trn, digs);
  (void) mul_mp (q, trn, mu, x, digs);
  (void) sub_mp (q, trm, trm, trn, digs);
  (void) sub_mp (q, trm, trm, sigma, digs);
  (void) exp_mp (q, trm, trm, digs);
  (void) plus_one_mp (q, trm, trm, digs);
  (void) sub_mp (q, trn, y, x, digs);
  (void) mul_mp (q, trm, trm, trn, digs);
  (void) half_mp (q, &R[IX (0, digs)], trm, digs);
// Loop for n > 0
  MP_T *relerr = nil_mp (q, digs);
  MP_T *relneeded = EPS (q, digs);
  (void) div_mp (q, relneeded, relneeded, TOL_ROMBERG (q, digs), digs);
  INT_T adr0 = 0;
  INT_T n = 1;
// REAL_T h = (y - x) / 2;       // n=1, h = (y-x)/2^n
  MP_T *h = nil_mp (q, digs);
  (void) sub_mp (q, h, y, x, digs);
  (void) half_mp (q, h, h, digs);
//  REAL_T pow2 = 1;              // n=1; pow2 = 2^(n-1)
  MP_T *pow2 = lit_mp (q, 1, 0, digs);
  BOOL_T cont = A68_TRUE;
  while (cont) {
// while (n <= NITERMAX_ROMBERG && relerr > relneeded);
//  mp_romberg_iterations (R, *sigma, n, x, y, mu, p, h, pow2);
    ADDR_T pop_sp_2 = A68_SP;
    mp_romberg_iterations (q, R, sigma, n, x, y, mu, p, h, pow2, digs);
    A68_SP = pop_sp_2;
//  h /= 2;
    (void) half_mp (q, h, h, digs);
//  pow2 *= 2;
    (void) add_mp (q, pow2, pow2, pow2, digs);
//  adr0 = (n * (n + 1)) / 2;
    adr0 = (n * (n + 1)) / 2;
//  relerr = abs ((R[adr0 + n] - R[adr0 + n - 1]) / R[adr0 + n]);
    (void) sub_mp (q, trm, &R[IX (adr0 + n, digs)], &R[IX (adr0 + n - 1, digs)], digs);
    (void) div_mp (q, trm, trm, &R[IX (adr0 + n, digs)], digs);
    (void) abs_mp (q, relerr, trm, digs);
//  n++;
    n++; 
    A68_BOOL gt;
    (void) gt_mp (q, &gt, relerr, relneeded, digs);
    cont = (n <= NITERMAX_ROMBERG) && VALUE (&gt);
  }
// save Romberg estimate and free memory
// rho = R[adr0 + (n - 1)];
  (void) move_mp (rho, &R[IX (adr0 + n - 1, digs)], digs);
  a68_free (R);
  A68_SP = pop_sp;
}

//! @brief compute generalized incomplete gamma function I_{x,y}^{mu,p}
//
//   I_{x,y}^{mu,p} = integral from x to y of s^{p-1} * exp (-mu*s) ds
//
// This procedure computes (rho, sigma) described below.
// The approximated value of I_{x,y}^{mu,p} is I = rho * exp (sigma)
//
//   mu is a real number non equal to zero 
//     (in general we take mu = 1 or -1 but any nonzero real number is allowed)
//
//   x, y are two numbers with 0 <= x <= y <= +infinity,
//     (the setting y=+infinity is allowed only when mu > 0)
//
//   p is a real number > 0, p must be an integer when mu < 0.

void Dgamic_mp (NODE_T *q, MP_T * rho, MP_T * sigma, MP_T *x, MP_T *y, MP_T *mu, MP_T *p, int digs)
{
  ADDR_T pop_sp = A68_SP;
// Particular cases
  if (PLUS_INF_MP (x) && PLUS_INF_MP (y)) {
    SET_MP_ZERO (rho, digs);
    SET_MP_ZERO (sigma, digs);
    MP_STATUS (sigma) = (UNSIGNED_T) MP_STATUS (sigma) | MINUS_INF_MASK;
    A68_SP = pop_sp;
    return;
  } else if (same_mp (q, x, y, digs)) {
    SET_MP_ZERO (rho, digs);
    SET_MP_ZERO (sigma, digs);
    MP_STATUS (sigma) = (UNSIGNED_T) MP_STATUS (sigma) | MINUS_INF_MASK;
    A68_SP = pop_sp;
    return;
  }
  if (IS_ZERO_MP (x) && PLUS_INF_MP (y)) {
    set_mp (rho, 1, 0, digs);
    MP_T *lgam = nil_mp (q, digs);
    MP_T *lnmu = nil_mp (q, digs);
    (void) lngamma_mp (q, lgam, p, digs);
    (void) ln_mp (q, lnmu, mu, digs);
    (void) mul_mp (q, lnmu, p, lnmu, digs);
    (void) sub_mp (q, sigma, lgam, lnmu, digs);
    return;
  }
// Initialization
  MP_T *mx = nil_mp (q, digs);
  MP_T *nx = nil_mp (q, digs);
  MP_T *my = nil_mp (q, digs);
  MP_T *ny = nil_mp (q, digs);
  MP_T *mux = nil_mp (q, digs);
  MP_T *muy = nil_mp (q, digs);
// Initialization
// nx = (a68_isinf (x) ? a68_neginf () : -mu * x + p * ln (x));
  if (PLUS_INF_MP (x)) {
    SET_MP_ZERO (mx, digs);
    MP_STATUS (nx) = (UNSIGNED_T) MP_STATUS (nx) | MINUS_INF_MASK;
  } else {
    (void) mul_mp (q, mux, mu, x, digs);
    G_func_mp (q, mx, p, mux, digs);
    (void) ln_mp (q, nx, x, digs);
    (void) mul_mp (q, nx, p, nx, digs);
    (void) sub_mp (q, nx, nx, mux, digs);
  }
// ny = (a68_isinf (y) ? a68_neginf () : -mu * y + p * ln (y));
  if (PLUS_INF_MP (y)) {
    SET_MP_ZERO (my, digs);
    MP_STATUS (ny) = (UNSIGNED_T) MP_STATUS (ny) | MINUS_INF_MASK;
  } else {
    (void) mul_mp (q, muy, mu, y, digs);
    G_func_mp (q, my, p, muy, digs);
    (void) ln_mp (q, ny, y, digs);
    (void) mul_mp (q, ny, p, ny, digs);
    (void) sub_mp (q, ny, ny, muy, digs);
  }
// Compute (mA,nA) and (mB,nB) such as I_{x,y}^{mu,p} can be
// approximated by the difference A-B, where A >= B >= 0, A = mA*exp (nA) an 
// B = mB*exp (nB). When the difference involves more than one digit loss due to
// cancellation errors, the integral I_{x,y}^{mu,p} is evaluated using the
// Romberg approximation method.
  MP_T *mA = nil_mp (q, digs);
  MP_T *mB = nil_mp (q, digs);
  MP_T *nA = nil_mp (q, digs);
  MP_T *nB = nil_mp (q, digs);
  MP_T *trm = nil_mp (q, digs);
  if (MP_DIGIT (mu, 1) < 0) {
    (void) move_mp (mA, my, digs);
    (void) move_mp (nA, ny, digs);
    (void) move_mp (mB, mx, digs);
    (void) move_mp (nB, nx, digs);
    goto compute;
  }
  MP_T *pl = nil_mp (q, digs);
  A68_BOOL lt;
  if (PLUS_INF_MP (x)) {
    VALUE (&lt) = A68_TRUE;
  } else {
    (void) mul_mp (q, mux, mu, x, digs);
    (void) plim_mp (q, pl, mux, digs);
    (void) lt_mp (q, &lt, p, pl, digs);
  }
  if (VALUE (&lt)) {
    (void) move_mp (mA, mx, digs);
    (void) move_mp (nA, nx, digs);
    (void) move_mp (mB, my, digs);
    (void) move_mp (nB, ny, digs);
    goto compute;
  }
  if (PLUS_INF_MP (y)) {
    VALUE (&lt) = A68_TRUE;
  } else {
    (void) mul_mp (q, muy, mu, y, digs);
    (void) plim_mp (q, pl, muy, digs);
    (void) lt_mp (q, &lt, p, pl, digs);
  }
  if (VALUE (&lt)) {
//  mA = 1;
    set_mp (mA, 1, 0, digs);
//  nA = lgammaq (p) - p * log_double (mu);
    MP_T *lgam = nil_mp (q, digs);
    MP_T *lnmu = nil_mp (q, digs);
    (void) lngamma_mp (q, lgam, p, digs);
    (void) ln_mp (q, lnmu, mu, digs);
    (void) mul_mp (q, lnmu, p, lnmu, digs);
    (void) sub_mp (q, nA, lgam, lnmu, digs);
//  nB = fmax (nx, ny);
    A68_BOOL ge;
    if (MINUS_INF_MP (ny)) {
      VALUE (&ge) = A68_TRUE;
    } else {
      (void) ge_mp (q, &ge, nx, ny, digs);
    }
    if (VALUE (&ge)) {
      (void) move_mp (nB, nx, digs);
    } else {
      (void) move_mp (nB, ny, digs);
    }
//  mB = mx * exp_double (nx - nB) + my * exp_double (ny - nB);
    (void) sub_mp (q, trm, nx, nB, digs);
    (void) exp_mp (q, trm, trm, digs);
    (void) mul_mp (q, mB, mx, trm, digs);
    if (! MINUS_INF_MP (ny)) {
      (void) sub_mp (q, trm, ny, nB, digs);
      (void) exp_mp (q, trm, trm, digs);
      (void) mul_mp (q, trm, my, trm, digs);
      (void) add_mp (q, mB, nB, trm, digs);
    }
    goto compute;
  }
  (void) move_mp (mA, my, digs);
  (void) move_mp (nA, ny, digs);
  (void) move_mp (mB, mx, digs);
  (void) move_mp (nB, nx, digs);
  compute:
// Compute (rho,sigma) such that rho*exp (sigma) = A-B
// 1. rho = mA - mB * exp_double (nB - nA);
  (void) sub_mp (q, trm, nB, nA, digs);
  (void) exp_mp (q, trm, trm, digs);
  (void) mul_mp (q, trm, mB, trm, digs);
  (void) sub_mp (q, rho, mA, trm, digs);
// 2. sigma = nA;
  (void) move_mp (sigma, nA, digs);
// If the difference involved a significant loss of precision, compute Romberg estimate.
// if (!isinf_double (y) && ((*rho) / mA < TOL_DIFF)) {
  (void) div_mp (q, trm, rho, mA, digs);
  (void) lt_mp (q, &lt, trm, TOL_DIFF (q, digs), digs);
  if (!PLUS_INF_MP (y) && VALUE (&lt)) {
    mp_romberg_estimate (q, rho, sigma, x, y, mu, p, digs);
  }
  A68_SP = pop_sp;
}

void Dgamic_wrap_mp (NODE_T *q, MP_T * s, MP_T * rho, MP_T * sigma, MP_T *x, MP_T *y, MP_T *mu, MP_T *p, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = GAM_DIGITS (MAX_PRECISION);
  errno = 0;
  if (digs <= gdigs) {
    gdigs = GAM_DIGITS (digs);
    MP_T *rho_g = len_mp (q, rho, digs, gdigs);
    MP_T *sigma_g = len_mp (q, sigma, digs, gdigs);
    MP_T *x_g = len_mp (q, x, digs, gdigs);
    MP_T *y_g = len_mp (q, y, digs, gdigs);
    MP_T *mu_g = len_mp (q, mu, digs, gdigs);
    MP_T *p_g = len_mp (q, p, digs, gdigs);
    Dgamic_mp (q, rho_g, sigma_g, x_g, y_g, mu_g, p_g, gdigs);
    if (IS_ZERO_MP (rho_g) || MINUS_INF_MP (sigma_g)) {
      SET_MP_ZERO (s, digs);
    } else {
      (void) exp_mp (q, sigma_g, sigma_g, gdigs);
      (void) mul_mp (q, rho_g, rho_g, sigma_g, gdigs);
      (void) shorten_mp (q, s, digs, rho_g, gdigs);
    }
  } else {
    diagnostic (A68_MATH_WARNING, q, WARNING_MATH_PRECISION, MOID (q), CALL, NULL);
    MP_T *rho_g = cut_mp (q, rho, digs, gdigs);
    MP_T *sigma_g = cut_mp (q, sigma, digs, gdigs);
    MP_T *x_g = cut_mp (q, x, digs, gdigs);
    MP_T *y_g = cut_mp (q, y, digs, gdigs);
    MP_T *mu_g = cut_mp (q, mu, digs, gdigs);
    MP_T *p_g = cut_mp (q, p, digs, gdigs);
    Dgamic_mp (q, rho_g, sigma_g, x_g, y_g, mu_g, p_g, gdigs);
    if (IS_ZERO_MP (rho_g) || MINUS_INF_MP (sigma_g)) {
      SET_MP_ZERO (s, digs);
    } else {
      (void) exp_mp (q, sigma_g, sigma_g, gdigs);
      (void) mul_mp (q, rho_g, rho_g, sigma_g, gdigs);
      MP_T *tmp = nil_mp (q, MAX_PRECISION);
      (void) shorten_mp (q, tmp, MAX_PRECISION, rho_g, gdigs);
      (void) lengthen_mp (q, s, digs, tmp, MAX_PRECISION);
    }
  }
  PRELUDE_ERROR (errno != 0, q, ERROR_MATH, MOID (q));
  A68_SP = pop_sp;
}

//! @brief PROC long long gamma inc f = (LONG LONG REAL p, x) LONG LONG REAL

void genie_gamma_inc_f_mp (NODE_T *p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *s = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *mu = lit_mp (p, 1, 0, digs);
  MP_T *y = nil_mp (p, digs);
  MP_T *rho = nil_mp (p, digs);
  MP_T *sigma = nil_mp (p, digs);
  MP_STATUS (y) = (UNSIGNED_T) MP_STATUS (y) | PLUS_INF_MASK;
  Dgamic_wrap_mp (p, s, rho, sigma, x, y, mu, s, digs);
  A68_SP = pop_sp;
  A68_SP -= size;
}

//! @brief PROC long long gamma inc g = (LONG LONG REAL p, x, y, mu) LONG LONG REAL

void genie_gamma_inc_g_mp (NODE_T *p)
{
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *mu = (MP_T *) STACK_OFFSET (-size);
  MP_T *y = (MP_T *) STACK_OFFSET (-2 * size);
  MP_T *x = (MP_T *) STACK_OFFSET (-3 * size);
  MP_T *s = (MP_T *) STACK_OFFSET (-4 * size);
  MP_T *rho = nil_mp (p, digs);
  MP_T *sigma = nil_mp (p, digs);
  Dgamic_wrap_mp (p, s, rho, sigma, x, y, mu, s, digs);
  A68_SP = pop_sp;
  A68_SP -= 3 * size;
}

//! @brief PROC long long gamma inc gf = (LONG LONG REAL p, x, y, mu) LONG LONG REAL

void genie_gamma_inc_gf_mp (NODE_T *p)
{
// if x <= p: G(p,x) = exp (x-p*ln (|x|)) * integral over [0,|x|] of s^{p-1} * exp (-sign (x)*s) ds
// otherwise: G(p,x) = exp (x-p*ln (x)) * integral over [x,inf] of s^{p-1} * exp (-s) ds
  int digs = DIGITS (MOID (p)), size = SIZE (MOID (p));
  ADDR_T pop_sp = A68_SP;
  MP_T *x = (MP_T *) STACK_OFFSET (-size);
  MP_T *s = (MP_T *) STACK_OFFSET (-2 * size);
  int gdigs = GAM_DIGITS (MAX_PRECISION);
  errno = 0;
  if (digs <= gdigs) {
    gdigs = GAM_DIGITS (digs);
    MP_T *x_g = len_mp (p, x, digs, gdigs);
    MP_T *s_g = len_mp (p, s, digs, gdigs);
    MP_T *G = nil_mp (p, gdigs);
    G_func_mp (p, G, s_g, x_g, gdigs);
    PRELUDE_ERROR (errno != 0, p, ERROR_MATH, MOID (p));
    (void) shorten_mp (p, s, digs, G, gdigs);
  } else {
    diagnostic (A68_MATH_WARNING, p, WARNING_MATH_PRECISION, MOID (p), CALL, NULL);
    MP_T *x_g = cut_mp (p, x, digs, gdigs);
    MP_T *s_g = cut_mp (p, s, digs, gdigs);
    MP_T *G = nil_mp (p, gdigs);
    G_func_mp (p, G, s_g, x_g, gdigs);
    PRELUDE_ERROR (errno != 0, p, ERROR_MATH, MOID (p));
    MP_T *tmp = nil_mp (p, MAX_PRECISION);
    (void) shorten_mp (p, tmp, MAX_PRECISION, G, gdigs);
    (void) lengthen_mp (p, s, digs, tmp, MAX_PRECISION);
  }
  A68_SP = pop_sp - size;
}

//! @brief PROC long long gamma inc = (LONG LONG REAL p, x) LONG LONG REAL

void genie_gamma_inc_h_mp (NODE_T *p)
{
//
// #if defined (HAVE_GNU_MPFR) && (A68_LEVEL >= 3)
//   genie_gamma_inc_mpfr (p);
// #else
//   genie_gamma_inc_f_mp (p);
// #endif
//
   genie_gamma_inc_f_mp (p);
}
