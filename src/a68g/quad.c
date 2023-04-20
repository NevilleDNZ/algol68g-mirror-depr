//! @file quad.c
//!
//! @author (see below)
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
//! Fixed precision LONG LONG REAL/COMPLEX library.

// A68G has an own LONG LONG REAL library with variable precision.
// This fixed-length library serves as more economical double precision 
// to double real, for instance in computing the incomplete gamma function.
//
// This code is based on the HPA Library, a branch of the CCMath library.
// The CCMath library and derived HPA Library are free software under the terms 
// of the GNU Lesser General Public License version 2.1 or any later version.
//
// Sources:
//   [https://directory.fsf.org/wiki/HPAlib]
//   [http://download-mirror.savannah.gnu.org/releases/hpalib/]
//   [http://savannah.nongnu.org/projects/hpalib]
//
//   Copyright 2000 Daniel A. Atkinson  [DanAtk@aol.com]
//   Copyright 2004 Ivano Primi         [ivprimi@libero.it]  
//   Copyright 2023 Marcel van der Veer [algol68g@xs4all.nl] - A68G version.

// IEEE 754 floating point standard is assumed. 
// A quad real number is represented as:
//
//   Sign bit(s): 0 -> positive, 1 -> negative.
//   Exponent(e): 15-bit biased integer (bias = 16383).
//   Mantissa(m): 15 words of 16 bit length including leading 1. 
//
// The range of representable numbers is then given by
//
//   2^16384      > x > 2^[-16383] 
//   1.19*10^4932 > x > 1.68*10^-[4932]
// 
// Special values of the exponent are:
//
//   All 1 -> infinity (floating point overflow).
//   All 0 -> number equals zero. 
//
// Underflow in operations is handled by a flush to zero. Thus, a number 
// with the exponent zero and nonzero mantissa is invalid (not-a-number). 
// A complex number is a structure formed by two REAL*32 numbers.
//
// HPA cannot extend precision beyond the preset, hardcoded precision.
// Hence some math routines will not achieve full precision.

#include "a68g.h"

#if (A68_LEVEL >= 3)

#include "a68g-quad.h"

// Constants, extended with respect to original HPA lib.

const int_2 QUAD_REAL_BIAS = 16383;
const int_2 QUAD_REAL_DBL_BIAS = 15360;
const int_2 QUAD_REAL_DBL_LEX = 12;
const int_2 QUAD_REAL_DBL_MAX = 2047;
const int_2 QUAD_REAL_K_LIN = -8 * FLT256_LEN;
const int_2 QUAD_REAL_MAX_P = 16 * FLT256_LEN;
const QUAD_T QUAD_REAL_E2MAX = {{0x400c, 0xfffb}};    // +16382.75 
const QUAD_T QUAD_REAL_E2MIN = {{0xc00c, 0xfffb}};    // -16382.75 
const QUAD_T QUAD_REAL_EMAX = {{0x400c, 0xb16c}};
const QUAD_T QUAD_REAL_EMIN = {{0xc00c, 0xb16c}};
const QUAD_T QUAD_REAL_MINF = {{0xffff, 0x0}};
const QUAD_T QUAD_REAL_PINF = {{0x7fff, 0x0}};
const QUAD_T QUAD_REAL_VGV = {{0x4013, 0x8000}};
const QUAD_T QUAD_REAL_VSV = {{0x3ff2, 0x8000}};
const unt_2 QUAD_REAL_M_EXP = 0x7fff;
const unt_2 QUAD_REAL_M_SIGN = 0x8000;

const QUAD_T QUAD_REAL_ZERO =
  {{0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_TENTH =
  {{0x3ffb, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc, 0xcccc}};
const QUAD_T QUAD_REAL_HALF = 
  {{0x3ffe, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_ONE =
  {{0x3fff, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_TWO =
  {{0x4000, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_TEN =
  {{0x4002, 0xa000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_HUNDRED =
  {{0x4005, 0xc800, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_THOUSAND =
  {{0x4008, 0xfa00, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_PI4 = 
  {{0x3ffe, 0xc90f, 0xdaa2, 0x2168, 0xc234, 0xc4c6, 0x628b, 0x80dc, 0x1cd1, 0x2902, 0x4e08, 0x8a67, 0xcc74, 0x020b, 0xbea6, 0x3b14}};
const QUAD_T QUAD_REAL_PI2 = 
  {{0x3fff, 0xc90f, 0xdaa2, 0x2168, 0xc234, 0xc4c6, 0x628b, 0x80dc, 0x1cd1, 0x2902, 0x4e08, 0x8a67, 0xcc74, 0x020b, 0xbea6, 0x3b14}};
const QUAD_T QUAD_REAL_PI = 
  {{0x4000, 0xc90f, 0xdaa2, 0x2168, 0xc234, 0xc4c6, 0x628b, 0x80dc, 0x1cd1, 0x2902, 0x4e08, 0x8a67, 0xcc74, 0x020b, 0xbea6, 0x3b14}};
const QUAD_T QUAD_REAL_LN2 = 
  {{0x3ffe, 0xb172, 0x17f7, 0xd1cf, 0x79ab, 0xc9e3, 0xb398, 0x03f2, 0xf6af, 0x40f3, 0x4326, 0x7298, 0xb62d, 0x8a0d, 0x175b, 0x8bab}};
const QUAD_T QUAD_REAL_LN10 = 
  {{0x4000, 0x935d, 0x8ddd, 0xaaa8, 0xac16, 0xea56, 0xd62b, 0x82d3, 0x0a28, 0xe28f, 0xecf9, 0xda5d, 0xf90e, 0x83c6, 0x1e82, 0x01f0}};
const QUAD_T QUAD_REAL_SQRT2 = 
  {{0x3fff, 0xb504, 0xf333, 0xf9de, 0x6484, 0x597d, 0x89b3, 0x754a, 0xbe9f, 0x1d6f, 0x60ba, 0x893b, 0xa84c, 0xed17, 0xac85, 0x8334}};
const QUAD_T QUAD_REAL_LOG2_E = 
  {{0x3fff, 0xb8aa, 0x3b29, 0x5c17, 0xf0bb, 0xbe87, 0xfed0, 0x691d, 0x3e88, 0xeb57, 0x7aa8, 0xdd69, 0x5a58, 0x8b25, 0x166c, 0xd1a1}};
const QUAD_T QUAD_REAL_LOG2_10 = 
  {{0x4000, 0xd49a, 0x784b, 0xcd1b, 0x8afe, 0x492b, 0xf6ff, 0x4daf, 0xdb4c, 0xd96c, 0x55fe, 0x37b3, 0xad4e, 0x91b6, 0xac80, 0x82e8}};
const QUAD_T QUAD_REAL_LOG10_E = 
  {{0x3ffd, 0xde5b, 0xd8a9, 0x3728, 0x7195, 0x355b, 0xaaaf, 0xad33, 0xdc32, 0x3ee3, 0x4602, 0x45c9, 0xa202, 0x3a3f, 0x2d44, 0xf78f}};
const QUAD_T QUAD_REAL_RNDCORR = 
  {{0x3ffe, 0x8000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x01ae}};
const QUAD_T QUAD_REAL_FIXCORR = 
  {{0x3f17, 0xc000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000}};
const QUAD_T QUAD_REAL_NAN = 
  {{0x0000, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff, 0xffff}};

static const char *errmsg[] = {
  "No error",
  "Division by zero",
  "Out of domain",
  "Bad exponent",
  "Floating point overflow",
  "Invalid error code"
};

static QUAD_T c_tan (QUAD_T u);
static QUAD_T rred (QUAD_T u, int i, int *p);

//! @brief _pi32

void _pi32 (QUAD_T *x)
{
  *x = QUAD_REAL_PI;
}

//! @brief sigerr_quad_real

int sigerr_quad_real (int errcond, int errcode, const char *where)
{
// errcode must come from the evaluation of an error condition.
// errcode, which should describe the type of the error, 
// should always be one between QUAD_REAL_EDIV, QUAD_REAL_EDOM, QUAD_REAL_EBADEXP and QUAD_REAL_FPOFLOW.  
  if (errcond == 0) {
    errcode = 0;
  }
  if (errcode < 0 || errcode > QUAD_REAL_NERR) {
    errcode = QUAD_REAL_EINV;
  }
  if (errcode != 0) {
    QUAD_RTE (where, errmsg[errcode]);
  }
  return errcode;
}

// Elementary stuff.

//! @brief neg_quad_real

inline QUAD_T neg_quad_real (QUAD_T s)
{
  unt_2 *p = (unt_2 *) &QV (s);
  *p ^= QUAD_REAL_M_SIGN;
  return s;
}

//! @brief abs_quad_real

inline QUAD_T abs_quad_real (QUAD_T s)
{
  unt_2 *p = (unt_2 *) &QV (s);
  *p &= QUAD_REAL_M_EXP;
  return s;
}

//! @brief getexp_quad_real

inline int getexp_quad_real (const QUAD_T * ps)
{
  unt_2 *q = (unt_2 *) &(ps->value);
  return (*q & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS;
}

//! @brief getsgn_quad_real

inline int getsgn_quad_real (const QUAD_T * ps)
{
  unt_2 *q = (unt_2 *) &(ps->value);
  return (*q & QUAD_REAL_M_SIGN);
}

//! @brief real_cmp_quad_real

int real_cmp_quad_real (const QUAD_T * pa, const QUAD_T * pb)
{
  unt_2 *p = (unt_2 *) &(pa->value), *q = (unt_2 *) &(pb->value);
  unt_2 p0, q0;
  int m;
  for (m = 1; m <= FLT256_LEN && p[m] == 0; m++);
  if (m > FLT256_LEN && (*p & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP) {
// *pa is actually zero 
    p0 = 0;
  } else {
    p0 = *p;
  }
  for (m = 1; m <= FLT256_LEN && q[m] == 0; m++);
  if (m > FLT256_LEN && (*q & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP) {
// *pb is actually zero 
    q0 = 0;
  } else {
    q0 = *q;
  }
  unt_2 e = p0 & QUAD_REAL_M_SIGN, k = q0 & QUAD_REAL_M_SIGN;
  if (e && !k) {
    return -1;
  } else if (!e && k) {
    return 1;
  } else {                        // *pa and *pb have the same sign 
    m = (e) ? -1 : 1;
    e = p0 & QUAD_REAL_M_EXP;
    k = q0 & QUAD_REAL_M_EXP;
    if (e > k) {
      return m;
    } else if (e < k) {
      return -m;
    } else {
      for (e = 0; *(++p) == *(++q) && e < FLT256_LEN; ++e);
      if (e < FLT256_LEN) {
        return (*p > *q ? m : -m);
      } else {
        return 0;
      }
    }
  }
}

//! @brief real_2_quad_real

QUAD_T real_2_quad_real (QUAD_T s, int m)
{
  unt_2 *p = (unt_2 *) &QV (s);
  int e;
  for (e = 1; e <= FLT256_LEN && p[e] == 0; e++);
  if (e <= FLT256_LEN) {
    e = *p & QUAD_REAL_M_EXP;            // biased exponent 
    if (e + m < 0) {
      return QUAD_REAL_ZERO;
    } else if ((sigerr_quad_real (e + m >= QUAD_REAL_M_EXP, QUAD_REAL_FPOFLOW, NULL))) {
      return ((s.value[0] & QUAD_REAL_M_SIGN) ? QUAD_REAL_MINF : QUAD_REAL_PINF);
    } else {
      *p += m;
      return s;
    }
  } else {                        // s is zero or +-Inf 
    return s;
  }
}

//! @brief sfmod_quad_real

QUAD_T sfmod_quad_real (QUAD_T s, int *p)
{
  unt_2 *pa = (unt_2 *) &QV (s);
  unt_2 *pb = pa + 1;
  int_2 e, k;
  e = (*pa & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS;
  if ((sigerr_quad_real (e >= 15, QUAD_REAL_FPOFLOW, NULL))) {
    *p = -1;
    return s;
  } else if (e < 0) {
    *p = 0;
    return s;
  }
  *p = *pb >> (15 - e);
  lshift_quad_real (++e, pb, FLT256_LEN);
  *pa -= e;
  for (e = 0; *pb == 0 && e < QUAD_REAL_MAX_P; ++pb, e += 16);
  if (e == QUAD_REAL_MAX_P) {
    return QUAD_REAL_ZERO;
  }
  for (k = 0; !((*pb << k) & QUAD_REAL_M_SIGN); ++k);
  if ((k += e)) {
    lshift_quad_real (k, pa + 1, FLT256_LEN);
    *pa -= k;
  }
  return s;
}

//! @brief lshift_quad_real

void lshift_quad_real (int n, unt_2 *pm, int m)
{
  unt_2 *pc = pm + m - 1;
  if (n < 16 * m) {
    unt_2 *pa = pm + n / 16;
    m = n % 16;
    n = 16 - m;
    while (pa < pc) {
      *pm = (*pa++) << m;
      *pm++ |= *pa >> n;
    }
    *pm++ = *pa << m;
  }
  while (pm <= pc) {
    *pm++ = 0;
  }
}

//! @brief rshift_quad_real

void rshift_quad_real (int n, unt_2 *pm, int m)
{
  unt_2 *pc = pm + m - 1;
  if (n < 16 * m) {
    unt_2 *pa = pc - n / 16;
    m = n % 16;
    n = 16 - m;
    while (pa > pm) {
      *pc = (*pa--) >> m;
      *pc-- |= *pa << n;
    }
    *pc-- = *pa >> m;
  }
  while (pc >= pm) {
    *pc-- = 0;
  }
}

//! @brief nint_quad_real

QUAD_T nint_quad_real (QUAD_T x)
{
  if (getsgn_quad_real (&x) == 0) {
    return floor_quad_real (add_quad_real (x, QUAD_REAL_HALF, A68_FALSE));
  } else {
    return neg_quad_real (floor_quad_real (add_quad_real (QUAD_REAL_HALF, x, A68_TRUE)));
  }
}

//! @brief aint_quad_real

QUAD_T aint_quad_real (QUAD_T x) 
{
  if (getsgn_quad_real (&x) == 0) {
    return trunc_quad_real (x);
  } else {
    return neg_quad_real (trunc_quad_real (x));
  }
}

//! @brief max_quad_real_2

QUAD_T max_quad_real_2 (QUAD_T a, QUAD_T b) 
{
  if (ge_quad_real (a, b)) {
    return a;
  } else {
    return b;
  }
}

//! @brief min_quad_real_2

QUAD_T min_quad_real_2 (QUAD_T a, QUAD_T b) 
{
  if (le_quad_real (a, b)) {
    return a;
  } else {
    return b;
  }
}

//! @brief mod_quad_real

QUAD_T mod_quad_real (QUAD_T a, QUAD_T b)
{
  QUAD_T q = div_quad_real (a, b);
  if (sgn_quad_real (&q) >= 0) {
    q = floor_quad_real (q);
  } else {
    q = neg_quad_real (floor_quad_real (neg_quad_real (q)));
  }
  return add_quad_real (a, mul_quad_real (b, q), 1);
}

//! @brief add_quad_real

QUAD_T add_quad_real (QUAD_T s, QUAD_T t, int f)
{
  REAL32 pe;
  unt_2 *pc, *pf = pe;
  unt_2 *pa = (unt_2 *) &QV (s);
  unt_2 *pb = (unt_2 *) &QV (t);
  int_2 e = *pa & QUAD_REAL_M_EXP;
  int_2 k = *pb & QUAD_REAL_M_EXP;
  if (f != 0) {
    *pb ^= QUAD_REAL_M_SIGN;
  }
  unt_2 u = (*pb ^ *pa) & QUAD_REAL_M_SIGN;
  f = 0;
  if (e > k) {
    if ((k = e - k) >= QUAD_REAL_MAX_P) {
      return s;
    }
    rshift_quad_real (k, pb + 1, FLT256_LEN);
  } else if (e < k) {
    if ((e = k - e) >= QUAD_REAL_MAX_P) {
      return t;
    }
    rshift_quad_real (e, pa + 1, FLT256_LEN);
    e = k;
    pc = pa;
    pa = pb;
    pb = pc;
  } else if (u != 0) {
    for (pc = pa, pf = pb; *(++pc) == *(++pf) && f < FLT256_LEN; ++f);
    if (f >= FLT256_LEN) {
      return QUAD_REAL_ZERO;
    }
    if (*pc < *pf) {
      pc = pa;
      pa = pb;
      pb = pc;
    }
    pf = pe + f;
  }
  unt_2 h = *pa & QUAD_REAL_M_SIGN;
  unt n = 0;
  if (u != 0) {
    for (pc = pb + FLT256_LEN; pc > pb; --pc) {
      *pc = ~(*pc);
    }
    n = 1L;
  }
  for (pc = pe + FLT256_LEN, pa += FLT256_LEN, pb += FLT256_LEN; pc > pf;) {
    n += *pa;
    pa--;
    n += *pb;
    pb--;
    *pc = n;
    pc--;
    n >>= 16;
  }
  if (u != 0) {
    for (; *(++pc) == 0; ++f);
    for (k = 0; !((*pc << k) & QUAD_REAL_M_SIGN); ++k);
    if ((k += 16 * f)) {
      if ((e -= k) <= 0) {
        return QUAD_REAL_ZERO;
      }
      lshift_quad_real (k, pe + 1, FLT256_LEN);
    }
  } else {
    if (n != 0) {
      ++e;
      if ((sigerr_quad_real (e == (short) QUAD_REAL_M_EXP, QUAD_REAL_FPOFLOW, NULL))) {
        return (!h ? QUAD_REAL_PINF : QUAD_REAL_MINF);
      }
      ++pf;
      rshift_quad_real (1, pf, FLT256_LEN);
      *pf |= QUAD_REAL_M_SIGN;
    }
  }
  *pe = e;
  *pe |= h;
  return *(QUAD_T *) pe;
}

//! @brief mul_quad_real

QUAD_T mul_quad_real (QUAD_T s, QUAD_T t)
{
  unt_2 pe[FLT256_LEN + 2], *q0, *q1, h;
  unt_2 *pa, *pb, *pc;
  unt m, n, p;
  int_2 e;
  int_2 k;
  q0 = (unt_2 *) &QV (s);
  q1 = (unt_2 *) &QV (t);
  e = (*q0 & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS;
  k = (*q1 & QUAD_REAL_M_EXP) + 1;
  if ((sigerr_quad_real (e > (short) QUAD_REAL_M_EXP - k, QUAD_REAL_FPOFLOW, NULL))) {
    return (((s.value[0] & QUAD_REAL_M_SIGN) ^ (t.value[0] & QUAD_REAL_M_SIGN)) ? QUAD_REAL_MINF : QUAD_REAL_PINF);
  }
  if ((e += k) <= 0) {
    return QUAD_REAL_ZERO;
  }
  h = (*q0 ^ *q1) & QUAD_REAL_M_SIGN;
  for (++q1, k = FLT256_LEN, p = n = 0L, pc = pe + FLT256_LEN + 1; k > 0; --k) {
    for (pa = q0 + k, pb = q1; pa > q0;) {
      m = *pa--;
      m *= *pb++;
      n += (m & 0xffffL);
      p += (m >> 16);
    }
    *pc-- = n;
    n = p + (n >> 16);
    p = 0L;
  }
  *pc = n;
  if (!(*pc & QUAD_REAL_M_SIGN)) {
    --e;
    if (e <= 0) {
      return QUAD_REAL_ZERO;
    }
    lshift_quad_real (1, pc, FLT256_LEN + 1);
  }
  if ((sigerr_quad_real (e == (short) QUAD_REAL_M_EXP, QUAD_REAL_FPOFLOW, NULL))) {
    return (!h ? QUAD_REAL_PINF : QUAD_REAL_MINF);
  }
  *pe = e;
  *pe |= h;
  return *(QUAD_T *) pe;
}

//! @brief div_quad_real

QUAD_T div_quad_real (QUAD_T s, QUAD_T t)
{
  QUAD_T a;
  unt_2 *pc, e, i;
  pc = (unt_2 *) &QV (t);
  e = *pc;
  *pc = QUAD_REAL_BIAS;
  if ((sigerr_quad_real (real_cmp_quad_real (&t, &QUAD_REAL_ZERO) == 0, QUAD_REAL_EDIV, "div_quad_real"))) {
    return QUAD_REAL_ZERO;
  } else {
    a = real_to_quad_real (1 / quad_real_to_real (t));
    *pc = e;
    pc = (unt_2 *) &QV (a);
    *pc += QUAD_REAL_BIAS - (e & QUAD_REAL_M_EXP);
    *pc |= e & QUAD_REAL_M_SIGN;
    for (i = 0; i < QUAD_REAL_ITT_DIV; ++i) {
      a = mul_quad_real (a, add_quad_real (QUAD_REAL_TWO, mul_quad_real (a, t), 1));
    }
    return mul_quad_real (s, a);
  }
}

//! @brief exp2_quad_real

QUAD_T exp2_quad_real (QUAD_T x)
{
  QUAD_T s, d, f;
  unt_2 *pf = (unt_2 *) &QV (x);
  int m, k;
  if (real_cmp_quad_real (&x, &QUAD_REAL_E2MIN) < 0) {
    return QUAD_REAL_ZERO;
  } else if ((sigerr_quad_real (real_cmp_quad_real (&x, &QUAD_REAL_E2MAX) > 0, QUAD_REAL_FPOFLOW, NULL))) {
    return QUAD_REAL_PINF;
  } else {
    m = (*pf & QUAD_REAL_M_SIGN) ? 1 : 0;
    x = sfmod_quad_real (x, &k);
    if (m != 0) {
      k *= -1;
    }
// -QUAD_REAL_BIAS <= k <= +QUAD_REAL_BIAS 
    x = mul_quad_real (x, QUAD_REAL_LN2);
    if (getexp_quad_real (&x) > -QUAD_REAL_BIAS) {
      x = real_2_quad_real (x, -1);
      s = mul_quad_real (x, x);
      f = QUAD_REAL_ZERO;
      for (d = int_to_quad_real (m = QUAD_REAL_MS_EXP); m > 1; m -= 2, d = int_to_quad_real (m)) {
        f = div_quad_real (s, _add_quad_real_ (d, f));
      }
      f = div_quad_real (x, _add_quad_real_ (d, f));
      f = div_quad_real (_add_quad_real_ (d, f), _sub_quad_real_ (d, f));
    } else {
      f = QUAD_REAL_ONE;
    }
    pf = (unt_2 *) &QV (f);
    if (-k > *pf) {
      return QUAD_REAL_ZERO;
    } else {
      *pf += k;
      if ((sigerr_quad_real (*pf >= QUAD_REAL_M_EXP, QUAD_REAL_FPOFLOW, NULL))) {
        return QUAD_REAL_PINF;
      } else {
        return f;
      }
    }
  }
}

//! @brief exp_quad_real

QUAD_T exp_quad_real (QUAD_T z)
{
  return exp2_quad_real (mul_quad_real (z, QUAD_REAL_LOG2_E));
}

//! @brief exp10_quad_real

QUAD_T exp10_quad_real (QUAD_T z)
{
  return exp2_quad_real (mul_quad_real (z, QUAD_REAL_LOG2_10));
}

//! @brief fmod_quad_real

QUAD_T fmod_quad_real (QUAD_T s, QUAD_T t, QUAD_T * q)
{
  if ((sigerr_quad_real (real_cmp_quad_real (&t, &QUAD_REAL_ZERO) == 0, QUAD_REAL_EDIV, "fmod_quad_real"))) {
    return QUAD_REAL_ZERO;
  } else {
    unt_2 *p, mask = 0xffff;
    int_2 e, i;
    int u;
    *q = div_quad_real (s, t);
    p = (unt_2 *) &(q->value);
    u = (*p & QUAD_REAL_M_SIGN) ? 0 : 1;
    e = (*p &= QUAD_REAL_M_EXP);         // biased exponent of *q 
    e = e < QUAD_REAL_BIAS ? 0 : e - QUAD_REAL_BIAS + 1;
    for (i = 1; e / 16 > 0; i++, e -= 16);
    if (i <= FLT256_LEN) {
// e = 0, ..., 15 
      mask <<= 16 - e;
      p[i] &= mask;
      for (i++; i <= FLT256_LEN; p[i] = 0, i++);
    }
// Now *q == abs(quotient of (s/t)) 
    return add_quad_real (s, mul_quad_real (t, *q), u);
  }
}

//! @brief frexp_quad_real

QUAD_T frexp_quad_real (QUAD_T s, int *p)
{
  unt_2 *ps = (unt_2 *) &QV (s), u;
  *p = (*ps & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS + 1;
  u = *ps & QUAD_REAL_M_SIGN;
  *ps = QUAD_REAL_BIAS - 1;
  *ps |= u;
  return s;
}

//! @brief nullify

static void nullify (int skip, unt_2 *p, int k)
{
// After skipping the first 'skip' bytes of the vector 'p',
// this function nullifies all the remaining ones. 'k' is
// the number of words forming the vector p.
// Warning: 'skip' must be positive !  
  int i;
  unt_2 mask = 0xffff;
  for (i = 0; skip / 16 > 0; i++, skip -= 16);
  if (i < k) {
// skip = 0, ..., 15 
    mask <<= 16 - skip;
    p[i] &= mask;
    for (i++; i < k; p[i] = 0, i++);
  }
}

//! @brief canonic_form

static void canonic_form (QUAD_T * px)
{
  unt_2 *p, u;
  int_2 e, i, j, skip;
  p = (unt_2 *) &(px->value);
  e = (*p & QUAD_REAL_M_EXP);            // biased exponent of x 
  u = (*p & QUAD_REAL_M_SIGN);            // sign of x            
  if (e < QUAD_REAL_BIAS - 1) {
    return;
  } else {
    unt_2 mask = 0xffff;
// e >= QUAD_REAL_BIAS - 1 
    for (i = 1, skip = e + 1 - QUAD_REAL_BIAS; skip / 16 > 0; i++, skip -= 16);
    if (i <= FLT256_LEN) {
// skip = 0, ..., 15 
      mask >>= skip;
      if ((p[i] & mask) != mask) {
        return;
      } else {
        for (j = i + 1; j <= FLT256_LEN && p[j] == 0xffff; j++);
        if (j > FLT256_LEN) {
          p[i] -= mask;
          for (j = i + 1; j <= FLT256_LEN; p[j] = 0, j++);
          if (!(p[1] & 0x8000)) {
            p[1] = 0x8000;
            *p = ++e;
            *p |= u;
          } else if (u != 0) {
            *px = add_quad_real (*px, QUAD_REAL_ONE, 1);
          } else {
            *px = add_quad_real (*px, QUAD_REAL_ONE, 0);
          }
        }
      }
    }
  }
}

//! @brief frac_quad_real

QUAD_T frac_quad_real (QUAD_T x)
{
// frac_quad_real(x) returns the fractional part of the number x.
// frac_quad_real(x) has the same sign as x.  
  unt_2 u, *p;
  int_2 e;
  int n;
  canonic_form (&x);
  p = (unt_2 *) &QV (x);
  e = (*p & QUAD_REAL_M_EXP);            // biased exponent of x 
  if (e < QUAD_REAL_BIAS) {
    return x;                   // The integer part of x is zero 
  } else {
    u = *p & QUAD_REAL_M_SIGN;            // sign of x 
    n = e - QUAD_REAL_BIAS + 1;
    lshift_quad_real (n, p + 1, FLT256_LEN);
    e = QUAD_REAL_BIAS - 1;
// Now I have to take in account the rule 
// of the leading one.                    
    while (e > 0 && !(p[1] & QUAD_REAL_M_SIGN)) {
      lshift_quad_real (1, p + 1, FLT256_LEN);
      e -= 1;
    }
// Now p+1 points to the fractionary part of x, 
// u is its sign, e is its biased exponent.     
    p[0] = e;
    p[0] |= u;
    return *(QUAD_T *) p;
  }
}

//! @brief trunc_quad_real

QUAD_T trunc_quad_real (QUAD_T x)
{
// trunc_quad_real(x) returns the integer part of the number x.
// trunc_quad_real(x) has the same sign as x.  
  unt_2 *p;
  int_2 e;
  canonic_form (&x);
  p = (unt_2 *) &QV (x);
  e = (*p & QUAD_REAL_M_EXP);            // biased exponent of x 
  if (e < QUAD_REAL_BIAS) {
    return QUAD_REAL_ZERO;               // The integer part of x is zero 
  } else {
    nullify (e - QUAD_REAL_BIAS + 1, p + 1, FLT256_LEN);
    return *(QUAD_T *) p;
  }
}

//! @brief round_quad_real

QUAD_T round_quad_real (QUAD_T x)
{
  return trunc_quad_real (add_quad_real (x, QUAD_REAL_RNDCORR, x.value[0] & QUAD_REAL_M_SIGN));
}

//! @brief ceil_quad_real

QUAD_T ceil_quad_real (QUAD_T x)
{
  unt_2 *ps = (unt_2 *) &QV (x);
  if ((*ps & QUAD_REAL_M_SIGN)) {
    return trunc_quad_real (x);
  } else {
    QUAD_T y = frac_quad_real (x);
// y has the same sign as x (see above). 
    return (real_cmp_quad_real (&y, &QUAD_REAL_ZERO) > 0 ? add_quad_real (trunc_quad_real (x), QUAD_REAL_ONE, 0) : x);
  }
}

//! @brief floor_quad_real

QUAD_T floor_quad_real (QUAD_T x)
{
  unt_2 *ps = (unt_2 *) &QV (x);
  if ((*ps & QUAD_REAL_M_SIGN)) {
    QUAD_T y = frac_quad_real (x);
// y has the same sign as x (see above). 
    return (real_cmp_quad_real (&y, &QUAD_REAL_ZERO) < 0 ? add_quad_real (trunc_quad_real (x), QUAD_REAL_ONE, 1) : x);
  } else {
    return trunc_quad_real (x);
  }
}

//! @brief add_correction_quad_real

static void add_correction_quad_real (QUAD_T * px, int k)
{
  int_2 e = (px->value[0] & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS;
//   e = (e > 0 ? e : 0);   
  *px = add_quad_real (*px, real_2_quad_real (QUAD_REAL_FIXCORR, e), k);
}

//! @brief fix_quad_real

QUAD_T fix_quad_real (QUAD_T x)
{
  unt_2 *p;
  int_2 e;
  add_correction_quad_real (&x, x.value[0] & QUAD_REAL_M_SIGN);
  p = (unt_2 *) &QV (x);
  e = (*p & QUAD_REAL_M_EXP);            // biased exponent of x 
  if (e < QUAD_REAL_BIAS) {
    return QUAD_REAL_ZERO;               // The integer part of x is zero 
  } else {
    nullify (e - QUAD_REAL_BIAS + 1, p + 1, FLT256_LEN);
    return *(QUAD_T *) p;
  }
}

//! @brief tanh_quad_real

QUAD_T tanh_quad_real (QUAD_T z)
{
  QUAD_T s, d, f;
  int m, k;
  if ((k = getexp_quad_real (&z)) > QUAD_REAL_K_TANH) {
    if (getsgn_quad_real (&z)) {
      return neg_quad_real (QUAD_REAL_ONE);
    } else {
      return QUAD_REAL_ONE;
    }
  }
  if (k < QUAD_REAL_K_LIN) {
    return z;
  }
  ++k;
  if (k > 0) {
    z = real_2_quad_real (z, -k);
  }
  s = mul_quad_real (z, z);
  f = QUAD_REAL_ZERO;
  for (d = int_to_quad_real (m = QUAD_REAL_MS_HYP); m > 1;) {
    f = div_quad_real (s, _add_quad_real_ (d, f));
    d = int_to_quad_real (m -= 2);
  }
  f = div_quad_real (z, _add_quad_real_ (d, f));
  for (; k > 0; --k) {
    f = div_quad_real (real_2_quad_real (f, 1), add_quad_real (d, mul_quad_real (f, f), 0));
  }
  return f;
}

//! @brief sinh_quad_real

QUAD_T sinh_quad_real (QUAD_T z)
{
  int k;
  if ((k = getexp_quad_real (&z)) < QUAD_REAL_K_LIN) {
    return z;
  } else if (k < 0) {
    z = tanh_quad_real (real_2_quad_real (z, -1));
    return div_quad_real (real_2_quad_real (z, 1), add_quad_real (QUAD_REAL_ONE, mul_quad_real (z, z), 1));
  } else {
    z = exp_quad_real (z);
    return real_2_quad_real (add_quad_real (z, div_quad_real (QUAD_REAL_ONE, z), 1), -1);
  }
}

//! @brief cosh_quad_real

QUAD_T cosh_quad_real (QUAD_T z)
{
  if (getexp_quad_real (&z) < QUAD_REAL_K_LIN) {
    return QUAD_REAL_ONE;
  }
  z = exp_quad_real (z);
  return real_2_quad_real (add_quad_real (z, div_quad_real (QUAD_REAL_ONE, z), 0), -1);
}

//! @brief atanh_quad_real

QUAD_T atanh_quad_real (QUAD_T x)
{
  QUAD_T y = x;
  y.value[0] &= QUAD_REAL_M_EXP;           // Now y == abs(x) 
  if ((sigerr_quad_real (real_cmp_quad_real (&y, &QUAD_REAL_ONE) >= 0, QUAD_REAL_EDOM, "xatanh"))) {
    return ((x.value[0] & QUAD_REAL_M_SIGN) ? QUAD_REAL_MINF : QUAD_REAL_PINF);
  } else {
    y = div_quad_real (add_quad_real (QUAD_REAL_ONE, x, 0), add_quad_real (QUAD_REAL_ONE, x, 1));
    return real_2_quad_real (log_quad_real (y), -1);
  }
}

//! @brief asinh_quad_real

QUAD_T asinh_quad_real (QUAD_T x)
{
  QUAD_T y = mul_quad_real (x, x);
  y = sqrt_quad_real (add_quad_real (QUAD_REAL_ONE, y, 0));
  if ((x.value[0] & QUAD_REAL_M_SIGN)) {
    return neg_quad_real (log_quad_real (_sub_quad_real_ (y, x)));
  } else {
    return log_quad_real (_add_quad_real_ (x, y));
  }
}

//! @brief acosh_quad_real

QUAD_T acosh_quad_real (QUAD_T x)
{
  if ((sigerr_quad_real (real_cmp_quad_real (&x, &QUAD_REAL_ONE) < 0, QUAD_REAL_EDOM, "acosh_quad_real"))) {
    return QUAD_REAL_ZERO;
  } else {
    QUAD_T y = mul_quad_real (x, x);
    y = sqrt_quad_real (add_quad_real (y, QUAD_REAL_ONE, 1));
    return log_quad_real (_add_quad_real_ (x, y));
  }
}

//! @brief atan_quad_real

QUAD_T atan_quad_real (QUAD_T z)
{
  QUAD_T s, f;
  int k, m;
  if ((k = getexp_quad_real (&z)) < QUAD_REAL_K_LIN) {
    return z;
  }
  if (k >= 0) {
// k>=0 is equivalent to abs(z) >= 1.0 
    z = div_quad_real (QUAD_REAL_ONE, z);
    m = 1;
  } else {
    m = 0;
  }
  f = real_to_quad_real (atan (quad_real_to_real (z)));
  s = add_quad_real (QUAD_REAL_ONE, mul_quad_real (z, z), 0);
  for (k = 0; k < QUAD_REAL_ITT_DIV; ++k) {
    f = add_quad_real (f, div_quad_real (add_quad_real (z, tan_quad_real (f), 1), s), 0);
  }
  if (m != 0) {
    if (getsgn_quad_real (&f)) {
      return add_quad_real (neg_quad_real (QUAD_REAL_PI2), f, 1);
    } else {
      return add_quad_real (QUAD_REAL_PI2, f, 1);
    }
  } else {
    return f;
  }
}

//! @brief asin_quad_real

QUAD_T asin_quad_real (QUAD_T z)
{
  QUAD_T u = z;
  u.value[0] &= QUAD_REAL_M_EXP;
  if ((sigerr_quad_real (real_cmp_quad_real (&u, &QUAD_REAL_ONE) > 0, QUAD_REAL_EDOM, "asin_quad_real"))) {
    return ((getsgn_quad_real (&z)) ? neg_quad_real (QUAD_REAL_PI2) : QUAD_REAL_PI2);
  } else {
    if (getexp_quad_real (&z) < QUAD_REAL_K_LIN) {
      return z;
    }
    u = sqrt_quad_real (add_quad_real (QUAD_REAL_ONE, mul_quad_real (z, z), 1));
    if (getexp_quad_real (&u) == -QUAD_REAL_BIAS) {
      return ((getsgn_quad_real (&z)) ? neg_quad_real (QUAD_REAL_PI2) : QUAD_REAL_PI2);
    }
    return atan_quad_real (div_quad_real (z, u));
  }
}

//! @brief acos_quad_real

QUAD_T acos_quad_real (QUAD_T z)
{
  QUAD_T u = z;
  u.value[0] &= QUAD_REAL_M_EXP;
  if ((sigerr_quad_real (real_cmp_quad_real (&u, &QUAD_REAL_ONE) > 0, QUAD_REAL_EDOM, "acos_quad_real"))) {
    return ((getsgn_quad_real (&z)) ? QUAD_REAL_PI : QUAD_REAL_ZERO);
  } else {
    if (getexp_quad_real (&z) == -QUAD_REAL_BIAS) {
      return QUAD_REAL_PI2;
    }
    u = sqrt_quad_real (add_quad_real (QUAD_REAL_ONE, mul_quad_real (z, z), 1));
    u = atan_quad_real (div_quad_real (u, z));
    if (getsgn_quad_real (&z)) {
      return add_quad_real (QUAD_REAL_PI, u, 0);
    } else {
      return u;
    }
  }
}

//! @brief atan2_quad_real

QUAD_T atan2_quad_real (QUAD_T y, QUAD_T x)
{
  int rs, is;
  rs = sgn_quad_real (&x);
  is = sgn_quad_real (&y);
  if (rs > 0) {
    return atan_quad_real (div_quad_real (y, x));
  } else if (rs < 0) {
    x.value[0] ^= QUAD_REAL_M_SIGN;
    y.value[0] ^= QUAD_REAL_M_SIGN;
    if (is >= 0) {
      return add_quad_real (QUAD_REAL_PI, atan_quad_real (div_quad_real (y, x)), 0);
    } else {
      return add_quad_real (atan_quad_real (div_quad_real (y, x)), QUAD_REAL_PI, 1);
    }
  } else {                      // x is zero ! 
    if (!sigerr_quad_real (is == 0, QUAD_REAL_EDOM, "atan2_quad_real")) {
      return (is > 0 ? QUAD_REAL_PI2 : neg_quad_real (QUAD_REAL_PI2));
    } else {
      return QUAD_REAL_ZERO;             // Dummy value :) 
    }
  }
}

//! @brief log_quad_real

QUAD_T log_quad_real (QUAD_T z)
{
  QUAD_T f, h;
  int k, m;
  if ((sigerr_quad_real ((getsgn_quad_real (&z)) || getexp_quad_real (&z) == -QUAD_REAL_BIAS, QUAD_REAL_EDOM, "log_quad_real"))) {
    return QUAD_REAL_MINF;
  } else if (real_cmp_quad_real (&z, &QUAD_REAL_ONE) == 0) {
    return QUAD_REAL_ZERO;
  } else {
    z = frexp_quad_real (z, &m);
    z = mul_quad_real (z, QUAD_REAL_SQRT2);
    z = div_quad_real (add_quad_real (z, QUAD_REAL_ONE, 1), add_quad_real (z, QUAD_REAL_ONE, 0));
    h = real_2_quad_real (z, 1);
    z = mul_quad_real (z, z);
    for (f = h, k = 1; getexp_quad_real (&h) > -QUAD_REAL_MAX_P;) {
      h = mul_quad_real (h, z);
      f = add_quad_real (f, div_quad_real (h, int_to_quad_real (k += 2)), 0);
    }
    return add_quad_real (f, mul_quad_real (QUAD_REAL_LN2, real_to_quad_real (m - .5)), 0);
  }
}

//! @brief log2_quad_real

QUAD_T log2_quad_real (QUAD_T z)
{
  QUAD_T f, h;
  int k, m;
  if ((sigerr_quad_real ((getsgn_quad_real (&z)) || getexp_quad_real (&z) == -QUAD_REAL_BIAS, QUAD_REAL_EDOM, "log2_quad_real"))) {
    return QUAD_REAL_MINF;
  } else if (real_cmp_quad_real (&z, &QUAD_REAL_ONE) == 0) {
    return QUAD_REAL_ZERO;
  } else {
    z = frexp_quad_real (z, &m);
    z = mul_quad_real (z, QUAD_REAL_SQRT2);
    z = div_quad_real (add_quad_real (z, QUAD_REAL_ONE, 1), add_quad_real (z, QUAD_REAL_ONE, 0));
    h = real_2_quad_real (z, 1);
    z = mul_quad_real (z, z);
    for (f = h, k = 1; getexp_quad_real (&h) > -QUAD_REAL_MAX_P;) {
      h = mul_quad_real (h, z);
      f = add_quad_real (f, div_quad_real (h, int_to_quad_real (k += 2)), 0);
    }
    return add_quad_real (mul_quad_real (f, QUAD_REAL_LOG2_E), real_to_quad_real (m - .5), 0);
  }
}

//! @brief log10_quad_real

QUAD_T log10_quad_real (QUAD_T z)
{
  QUAD_T w = log_quad_real (z);
  if (real_cmp_quad_real (&w, &QUAD_REAL_MINF) <= 0) {
    return QUAD_REAL_MINF;
  } else {
    return mul_quad_real (w, QUAD_REAL_LOG10_E);
  }
}
//! @brief eq_quad_real

int eq_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) == 0);
}

//! @brief neq_quad_real

int neq_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) != 0);
}

//! @brief gt_quad_real

int gt_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) > 0);
}

//! @brief ge_quad_real

int ge_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) >= 0);
}

//! @brief lt_quad_real

int lt_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) < 0);
}

//! @brief le_quad_real

int le_quad_real (QUAD_T x1, QUAD_T x2)
{
  return (real_cmp_quad_real (&x1, &x2) <= 0);
}
// isNaN_quad_real (&x) returns 1 if and only if x is not a valid number 

int isNaN_quad_real (const QUAD_T * u)
{
  unt_2 *p = (unt_2 *) &(u->value);
  if (*p != 0) {
    return 0;
  } else {
    int i;
    for (i = 1; i <= FLT256_LEN && p[i] == 0x0; i++);
    return (i <= FLT256_LEN ? 1 : 0);
  }
}

//! @brief is0_quad_real

int is0_quad_real (const QUAD_T * u)
{
  unt_2 *p = (unt_2 *) &(u->value);
  int m;
  for (m = 1; m <= FLT256_LEN && p[m] == 0; m++);
  return (m > FLT256_LEN && (*p & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP ? 1 : 0);
}

//! @brief not0_quad_real

int not0_quad_real (const QUAD_T * u)
{
  unt_2 *p = (unt_2 *) &(u->value);
  int m;
  for (m = 1; m <= FLT256_LEN && p[m] == 0; m++);
  return (m > FLT256_LEN && (*p & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP ? 0 : 1);
}

//! @brief sgn_quad_real

int sgn_quad_real (const QUAD_T * u)
{
  unt_2 *p = (unt_2 *) &(u->value);
  int m;
  for (m = 1; m <= FLT256_LEN && p[m] == 0; m++);
  if ((m > FLT256_LEN && (*p & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP) || !*p) {
    return 0;
  } else {
    return ((*p & QUAD_REAL_M_SIGN) ? -1 : 1);
  }
}

//! @brief isPinf_quad_real

int isPinf_quad_real (const QUAD_T * u)
{
  return (*u->value == QUAD_REAL_M_EXP ? 1 : 0);
}

//! @brief isMinf_quad_real

int isMinf_quad_real (const QUAD_T * u)
{
  return (*u->value == (QUAD_REAL_M_EXP | QUAD_REAL_M_SIGN) ? 1 : 0);
}

//! @brief isordnumb_quad_real

int isordnumb_quad_real (const QUAD_T * u)
{
  int isNaN, isfinite;
  unt_2 *p = (unt_2 *) &(u->value);
  if (*p != 0) {
    isNaN = 0;
  } else {
    int i;
    for (i = 1; i <= FLT256_LEN && p[i] == 0x0; i++);
    isNaN = i <= FLT256_LEN;
  }
  isfinite = (*p & QUAD_REAL_M_EXP) < QUAD_REAL_M_EXP;
  return (!isNaN && (isfinite) ? 1 : 0);
}

//! @brief pwr_quad_real

QUAD_T pwr_quad_real (QUAD_T s, int n)
{
  QUAD_T t;
  unsigned k, m;
  t = QUAD_REAL_ONE;
  if (n < 0) {
    m = -n;
    if ((sigerr_quad_real (real_cmp_quad_real (&s, &QUAD_REAL_ZERO) == 0, QUAD_REAL_EBADEXP, "pwr_quad_real"))) {
      return QUAD_REAL_ZERO;
    }
    s = div_quad_real (QUAD_REAL_ONE, s);
  } else {
    m = n;
  }
  if (m != 0) {
    k = 1;
    while (1) {
      if (k & m) {
        t = mul_quad_real (s, t);
      }
      if ((k <<= 1) <= m) {
        s = mul_quad_real (s, s);
      } else {
        break;
      }
    }
  } else {
    sigerr_quad_real (real_cmp_quad_real (&s, &QUAD_REAL_ZERO) == 0, QUAD_REAL_EBADEXP, "pwr_quad_real");
  }
  return t;
}

//! @brief pow_quad_real

QUAD_T pow_quad_real (QUAD_T x, QUAD_T y)
{
  if (sigerr_quad_real ((getsgn_quad_real (&x)) || getexp_quad_real (&x) == -QUAD_REAL_BIAS, QUAD_REAL_EDOM, "pow_quad_real")) {
    return QUAD_REAL_ZERO;
  } else {
    return exp2_quad_real (mul_quad_real (log2_quad_real (x), y));
  }
}

//! @brief sqrt_quad_real

QUAD_T sqrt_quad_real (QUAD_T z)
{
  if ((sigerr_quad_real ((getsgn_quad_real (&z)), QUAD_REAL_EDOM, "sqrt_quad_real"))) {
    return QUAD_REAL_ZERO;
  } else {
    unt_2 *pc = (unt_2 *) &QV (z);
    if (*pc == 0) {
      return QUAD_REAL_ZERO;
    }
    int_2 e = *pc - QUAD_REAL_BIAS;
    *pc = QUAD_REAL_BIAS + (e % 2);
    e /= 2;
    QUAD_T h, s = real_to_quad_real (sqrt (quad_real_to_real (z)));
    for (int_2 m = 0; m < QUAD_REAL_ITT_DIV; ++m) {
      h = div_quad_real (add_quad_real (z, mul_quad_real (s, s), 1), real_2_quad_real (s, 1));
      s = _add_quad_real_ (s, h);
    }
    pc = (unt_2 *) &QV (s);
    *pc += e;
    return s;
  }
}

static int odd_quad_real (QUAD_T x)
{
  unt_2 *p = (unt_2 *) &QV (x);
  int_2 e, i;
  e = (*p & QUAD_REAL_M_EXP) - QUAD_REAL_BIAS;    // exponent of x 
  if (e < 0) {
    return 0;
  } else {
    for (i = 1; e / 16 > 0; i++, e -= 16);
// Now e = 0, ..., 15 
    return (i <= FLT256_LEN ? p[i] & 0x8000 >> e : 0);
  }
}

//! @brief tan_quad_real

QUAD_T tan_quad_real (QUAD_T z)
{
  int k, m;
  z = rred (z, 't', &k);
  if ((sigerr_quad_real (real_cmp_quad_real (&z, &QUAD_REAL_PI2) >= 0, QUAD_REAL_EDOM, "tan_quad_real"))) {
    return (!k ? QUAD_REAL_PINF : QUAD_REAL_MINF);
  } else {
    if (real_cmp_quad_real (&z, &QUAD_REAL_PI4) == 1) {
      m = 1;
      z = add_quad_real (QUAD_REAL_PI2, z, 1);
    } else {
      m = 0;
    }
    if (k != 0) {
      z = neg_quad_real (c_tan (z));
    } else {
      z = c_tan (z);
    }
    if (m != 0) {
      return div_quad_real (QUAD_REAL_ONE, z);
    } else {
      return z;
    }
  }
}

//! @brief cos_quad_real

QUAD_T cos_quad_real (QUAD_T z)
{
  int k;
  z = rred (z, 'c', &k);
  if (getexp_quad_real (&z) < QUAD_REAL_K_LIN) {
    if (k != 0) {
      return neg_quad_real (QUAD_REAL_ONE);
    } else {
      return QUAD_REAL_ONE;
    }
  }
  z = c_tan (real_2_quad_real (z, -1));
  z = mul_quad_real (z, z);
  z = div_quad_real (add_quad_real (QUAD_REAL_ONE, z, 1), add_quad_real (QUAD_REAL_ONE, z, 0));
  if (k != 0) {
    return neg_quad_real (z);
  } else {
    return z;
  }
}

//! @brief sin_quad_real

QUAD_T sin_quad_real (QUAD_T z)
{
  int k;
  z = rred (z, 's', &k);
  if (getexp_quad_real (&z) >= QUAD_REAL_K_LIN) {
    z = c_tan (real_2_quad_real (z, -1));
    z = div_quad_real (real_2_quad_real (z, 1), add_quad_real (QUAD_REAL_ONE, mul_quad_real (z, z), 0));
  }
  if (k != 0) {
    return neg_quad_real (z);
  } else {
    return z;
  }
}

static QUAD_T c_tan (QUAD_T z)
{
  QUAD_T s, f, d;
  int m;
  unt_2 k;
  if (getexp_quad_real (&z) < QUAD_REAL_K_LIN)
    return z;
  s = neg_quad_real (mul_quad_real (z, z));
  for (k = 1; k <= FLT256_LEN && s.value[k] == 0; k++);
  if ((sigerr_quad_real (s.value[0] == 0xffff && k > FLT256_LEN, QUAD_REAL_FPOFLOW, NULL))) {
    return QUAD_REAL_ZERO;
  } else {
    f = QUAD_REAL_ZERO;
    for (d = int_to_quad_real (m = QUAD_REAL_MS_TRG); m > 1;) {
      f = div_quad_real (s, _add_quad_real_ (d, f));
      d = int_to_quad_real (m -= 2);
    }
    return div_quad_real (z, _add_quad_real_ (d, f));
  }
}

static QUAD_T rred (QUAD_T z, int kf, int *ps)
{
  QUAD_T is, q;
  if (getsgn_quad_real (&z)) {
    z = neg_quad_real (z);
    is = QUAD_REAL_ONE;
  } else {
    is = QUAD_REAL_ZERO;
  }
  z = fmod_quad_real (z, QUAD_REAL_PI, &q);
  if (kf == 't') {
    q = is;
  } else if (kf == 's') {
    q = _add_quad_real_ (q, is);
  }
  if (real_cmp_quad_real (&z, &QUAD_REAL_PI2) == 1) {
    z = add_quad_real (QUAD_REAL_PI, z, 1);
    if (kf == 'c' || kf == 't') {
      q = add_quad_real (q, QUAD_REAL_ONE, 0);
    }
  }
  *ps = (odd_quad_real (q)) ? 1 : 0;
  return z;
}

// VIF additions (REAL*32)

//! @brief cotan_quad_real

QUAD_T cotan_quad_real (QUAD_T x)
{
  return div_quad_real (QUAD_REAL_ONE, tan_quad_real (x));
}

//! @brief acotan_quad_real

QUAD_T acotan_quad_real (QUAD_T x)
{
  return atan_quad_real (div_quad_real (QUAD_REAL_ONE, x));
}

//! @brief sgn_quad_real_2

QUAD_T sgn_quad_real_2 (QUAD_T a, QUAD_T b)
{
  QUAD_T x = (getsgn_quad_real (&a) == 0 ? a : neg_quad_real (a));
  return (getsgn_quad_real (&b) == 0 ? x : neg_quad_real (x));
}

#endif
