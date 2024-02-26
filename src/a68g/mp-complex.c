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
//! [LONG] LONG COMPLEX math functions.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-mp.h"

//! @brief LONG COMPLEX multiplication

MP_T *cmul_mp (NODE_T * p, MP_T * a, MP_T * b, MP_T * c, MP_T * d, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *la = len_mp (p, a, digs, gdigs);
  MP_T *lb = len_mp (p, b, digs, gdigs);
  MP_T *lc = len_mp (p, c, digs, gdigs);
  MP_T *ld = len_mp (p, d, digs, gdigs);
  MP_T *ac = nil_mp (p, gdigs);
  MP_T *bd = nil_mp (p, gdigs);
  MP_T *ad = nil_mp (p, gdigs);
  MP_T *bc = nil_mp (p, gdigs);
  (void) mul_mp (p, ac, la, lc, gdigs);
  (void) mul_mp (p, bd, lb, ld, gdigs);
  (void) mul_mp (p, ad, la, ld, gdigs);
  (void) mul_mp (p, bc, lb, lc, gdigs);
  (void) sub_mp (p, la, ac, bd, gdigs);
  (void) add_mp (p, lb, ad, bc, gdigs);
  (void) shorten_mp (p, a, digs, la, gdigs);
  (void) shorten_mp (p, b, digs, lb, gdigs);
  A68_SP = pop_sp;
  return a;
}

//! @brief LONG COMPLEX division

MP_T *cdiv_mp (NODE_T * p, MP_T * a, MP_T * b, MP_T * c, MP_T * d, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if (MP_DIGIT (c, 1) == (MP_T) 0 && MP_DIGIT (d, 1) == (MP_T) 0) {
    errno = ERANGE;
    return NaN_MP;
  }
  MP_T *q = nil_mp (p, digs);
  MP_T *r = nil_mp (p, digs);
  (void) move_mp (q, c, digs);
  (void) move_mp (r, d, digs);
  MP_DIGIT (q, 1) = ABS (MP_DIGIT (q, 1));
  MP_DIGIT (r, 1) = ABS (MP_DIGIT (r, 1));
  (void) sub_mp (p, q, q, r, digs);
  if (MP_DIGIT (q, 1) >= 0) {
    if (div_mp (p, q, d, c, digs) == NaN_MP) {
      errno = ERANGE;
      return NaN_MP;
    }
    (void) mul_mp (p, r, d, q, digs);
    (void) add_mp (p, r, r, c, digs);
    (void) mul_mp (p, c, b, q, digs);
    (void) add_mp (p, c, c, a, digs);
    (void) div_mp (p, c, c, r, digs);
    (void) mul_mp (p, d, a, q, digs);
    (void) sub_mp (p, d, b, d, digs);
    (void) div_mp (p, d, d, r, digs);
  } else {
    if (div_mp (p, q, c, d, digs) == NaN_MP) {
      errno = ERANGE;
      return NaN_MP;
    }
    (void) mul_mp (p, r, c, q, digs);
    (void) add_mp (p, r, r, d, digs);
    (void) mul_mp (p, c, a, q, digs);
    (void) add_mp (p, c, c, b, digs);
    (void) div_mp (p, c, c, r, digs);
    (void) mul_mp (p, d, b, q, digs);
    (void) sub_mp (p, d, d, a, digs);
    (void) div_mp (p, d, d, r, digs);
  }
  (void) move_mp (a, c, digs);
  (void) move_mp (b, d, digs);
  A68_SP = pop_sp;
  return a;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX csqrt

MP_T *csqrt_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  if (IS_ZERO_MP (re) && IS_ZERO_MP (im)) {
    SET_MP_ZERO (re, gdigs);
    SET_MP_ZERO (im, gdigs);
  } else {
    MP_T *c1 = lit_mp (p, 1, 0, gdigs);
    MP_T *t = nil_mp (p, gdigs);
    MP_T *x = nil_mp (p, gdigs);
    MP_T *y = nil_mp (p, gdigs);
    MP_T *u = nil_mp (p, gdigs);
    MP_T *v = nil_mp (p, gdigs);
    MP_T *w = nil_mp (p, gdigs);
    SET_MP_ONE (c1, gdigs);
    (void) move_mp (x, re, gdigs);
    (void) move_mp (y, im, gdigs);
    MP_DIGIT (x, 1) = ABS (MP_DIGIT (x, 1));
    MP_DIGIT (y, 1) = ABS (MP_DIGIT (y, 1));
    (void) sub_mp (p, w, x, y, gdigs);
    if (MP_DIGIT (w, 1) >= 0) {
      (void) div_mp (p, t, y, x, gdigs);
      (void) mul_mp (p, v, t, t, gdigs);
      (void) add_mp (p, u, c1, v, gdigs);
      (void) sqrt_mp (p, v, u, gdigs);
      (void) add_mp (p, u, c1, v, gdigs);
      (void) half_mp (p, v, u, gdigs);
      (void) sqrt_mp (p, u, v, gdigs);
      (void) sqrt_mp (p, v, x, gdigs);
      (void) mul_mp (p, w, u, v, gdigs);
    } else {
      (void) div_mp (p, t, x, y, gdigs);
      (void) mul_mp (p, v, t, t, gdigs);
      (void) add_mp (p, u, c1, v, gdigs);
      (void) sqrt_mp (p, v, u, gdigs);
      (void) add_mp (p, u, t, v, gdigs);
      (void) half_mp (p, v, u, gdigs);
      (void) sqrt_mp (p, u, v, gdigs);
      (void) sqrt_mp (p, v, y, gdigs);
      (void) mul_mp (p, w, u, v, gdigs);
    }
    if (MP_DIGIT (re, 1) >= 0) {
      (void) move_mp (re, w, gdigs);
      (void) add_mp (p, u, w, w, gdigs);
      (void) div_mp (p, im, im, u, gdigs);
    } else {
      if (MP_DIGIT (im, 1) < 0) {
        MP_DIGIT (w, 1) = -MP_DIGIT (w, 1);
      }
      (void) add_mp (p, v, w, w, gdigs);
      (void) div_mp (p, re, im, v, gdigs);
      (void) move_mp (im, w, gdigs);
    }
  }
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX cexp

MP_T *cexp_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  MP_T *u = nil_mp (p, gdigs);
  (void) exp_mp (p, u, re, gdigs);
  (void) cos_mp (p, re, im, gdigs);
  (void) sin_mp (p, im, im, gdigs);
  (void) mul_mp (p, re, re, u, gdigs);
  (void) mul_mp (p, im, im, u, gdigs);
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX cln

MP_T *cln_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
  MP_T *u = nil_mp (p, gdigs);
  MP_T *v = nil_mp (p, gdigs);
  (void) move_mp (u, re, gdigs);
  (void) move_mp (v, im, gdigs);
  (void) hypot_mp (p, s, u, v, gdigs);
  (void) move_mp (u, re, gdigs);
  (void) move_mp (v, im, gdigs);
  (void) atan2_mp (p, t, u, v, gdigs);
  (void) ln_mp (p, re, s, gdigs);
  (void) move_mp (im, t, gdigs);
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX csin

MP_T *csin_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *c = nil_mp (p, gdigs);
  MP_T *sh = nil_mp (p, gdigs);
  MP_T *ch = nil_mp (p, gdigs);
  if (IS_ZERO_MP (im)) {
    (void) sin_mp (p, re, re, gdigs);
    SET_MP_ZERO (im, gdigs);
  } else {
    (void) sin_mp (p, s, re, gdigs);
    (void) cos_mp (p, c, re, gdigs);
    (void) hyp_mp (p, sh, ch, im, gdigs);
    (void) mul_mp (p, re, s, ch, gdigs);
    (void) mul_mp (p, im, c, sh, gdigs);
  }
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX ccos

MP_T *ccos_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *c = nil_mp (p, gdigs);
  MP_T *sh = nil_mp (p, gdigs);
  MP_T *ch = nil_mp (p, gdigs);
  if (IS_ZERO_MP (im)) {
    (void) cos_mp (p, re, re, gdigs);
    SET_MP_ZERO (im, gdigs);
  } else {
    (void) sin_mp (p, s, re, gdigs);
    (void) cos_mp (p, c, re, gdigs);
    (void) hyp_mp (p, sh, ch, im, gdigs);
    MP_DIGIT (sh, 1) = -MP_DIGIT (sh, 1);
    (void) mul_mp (p, re, c, ch, gdigs);
    (void) mul_mp (p, im, s, sh, gdigs);
  }
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX ctan

MP_T *ctan_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  errno = 0;
  MP_T *s = nil_mp (p, digs);
  MP_T *t = nil_mp (p, digs);
  MP_T *u = nil_mp (p, digs);
  MP_T *v = nil_mp (p, digs);
  (void) move_mp (u, r, digs);
  (void) move_mp (v, i, digs);
  (void) csin_mp (p, u, v, digs);
  (void) move_mp (s, u, digs);
  (void) move_mp (t, v, digs);
  (void) move_mp (u, r, digs);
  (void) move_mp (v, i, digs);
  (void) ccos_mp (p, u, v, digs);
  (void) cdiv_mp (p, s, t, u, v, digs);
  (void) move_mp (r, s, digs);
  (void) move_mp (i, t, digs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX casin

MP_T *casin_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  BOOL_T negim = MP_DIGIT (im, 1) < 0;
  if (IS_ZERO_MP (im)) {
    BOOL_T neg = MP_DIGIT (re, 1) < 0;
    if (acos_mp (p, im, re, gdigs) == NaN_MP) {
      errno = 0;                // Ignore the acos error
      MP_DIGIT (re, 1) = ABS (MP_DIGIT (re, 1));
      (void) acosh_mp (p, im, re, gdigs);
    }
    (void) mp_pi (p, re, MP_HALF_PI, gdigs);
    if (neg) {
      MP_DIGIT (re, 1) = -MP_DIGIT (re, 1);
    }
  } else {
    MP_T *c1 = lit_mp (p, 1, 0, gdigs);
    MP_T *u = nil_mp (p, gdigs);
    MP_T *v = nil_mp (p, gdigs);
    MP_T *a = nil_mp (p, gdigs);
    MP_T *b = nil_mp (p, gdigs);
// u=sqrt((r+1)^2+i^2), v=sqrt((r-1)^2+i^2).
    (void) add_mp (p, a, re, c1, gdigs);
    (void) sub_mp (p, b, re, c1, gdigs);
    (void) hypot_mp (p, u, a, im, gdigs);
    (void) hypot_mp (p, v, b, im, gdigs);
// a=(u+v)/2, b=(u-v)/2.
    (void) add_mp (p, a, u, v, gdigs);
    (void) half_mp (p, a, a, gdigs);
    (void) sub_mp (p, b, u, v, gdigs);
    (void) half_mp (p, b, b, gdigs);
// r=asin(b), i=ln(a+sqrt(a^2-1)).
    (void) mul_mp (p, u, a, a, gdigs);
    (void) sub_mp (p, u, u, c1, gdigs);
    (void) sqrt_mp (p, u, u, gdigs);
    (void) add_mp (p, u, a, u, gdigs);
    (void) ln_mp (p, im, u, gdigs);
    (void) asin_mp (p, re, b, gdigs);
  }
  if (negim) {
    MP_DIGIT (im, 1) = -MP_DIGIT (im, 1);
  }
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return re;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX cacos

MP_T *cacos_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  BOOL_T negim = MP_DIGIT (im, 1) < 0;
  if (IS_ZERO_MP (im)) {
    BOOL_T neg = MP_DIGIT (re, 1) < 0;
    if (acos_mp (p, im, re, gdigs) == NaN_MP) {
      errno = 0;                // Ignore the acos error
      MP_DIGIT (re, 1) = ABS (MP_DIGIT (re, 1));
      (void) acosh_mp (p, im, re, gdigs);
      MP_DIGIT (im, 1) = -MP_DIGIT (im, 1);
    }
    if (neg) {
      (void) mp_pi (p, re, MP_PI, gdigs);
    } else {
      SET_MP_ZERO (re, gdigs);
    }
  } else {
    MP_T *c1 = lit_mp (p, 1, 0, gdigs);
    MP_T *u = nil_mp (p, gdigs);
    MP_T *v = nil_mp (p, gdigs);
    MP_T *a = nil_mp (p, gdigs);
    MP_T *b = nil_mp (p, gdigs);
// u=sqrt((r+1)^2+i^2), v=sqrt((r-1)^2+i^2).
    (void) add_mp (p, a, re, c1, gdigs);
    (void) sub_mp (p, b, re, c1, gdigs);
    (void) hypot_mp (p, u, a, im, gdigs);
    (void) hypot_mp (p, v, b, im, gdigs);
// a=(u+v)/2, b=(u-v)/2.
    (void) add_mp (p, a, u, v, gdigs);
    (void) half_mp (p, a, a, gdigs);
    (void) sub_mp (p, b, u, v, gdigs);
    (void) half_mp (p, b, b, gdigs);
// r=acos(b), i=-ln(a+sqrt(a^2-1)).
    (void) mul_mp (p, u, a, a, gdigs);
    (void) sub_mp (p, u, u, c1, gdigs);
    (void) sqrt_mp (p, u, u, gdigs);
    (void) add_mp (p, u, a, u, gdigs);
    (void) ln_mp (p, im, u, gdigs);
    (void) acos_mp (p, re, b, gdigs);
  }
  if (!negim) {
    MP_DIGIT (im, 1) = -MP_DIGIT (im, 1);
  }
  (void) shorten_mp (p, r, digs, re, gdigs);
  (void) shorten_mp (p, i, digs, im, gdigs);
  A68_SP = pop_sp;
  return re;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX catan

MP_T *catan_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *re = len_mp (p, r, digs, gdigs);
  MP_T *im = len_mp (p, i, digs, gdigs);
  MP_T *u = nil_mp (p, gdigs);
  MP_T *v = nil_mp (p, gdigs);
  if (IS_ZERO_MP (im)) {
    (void) atan_mp (p, u, re, gdigs);
    SET_MP_ZERO (v, gdigs);
  } else {
    MP_T *c1 = lit_mp (p, 1, 0, gdigs);
    MP_T *a = nil_mp (p, gdigs);
    MP_T *b = nil_mp (p, gdigs);
// a=sqrt(r^2+(i+1)^2), b=sqrt(r^2+(i-1)^2).
    (void) add_mp (p, a, im, c1, gdigs);
    (void) sub_mp (p, b, im, c1, gdigs);
    (void) hypot_mp (p, u, re, a, gdigs);
    (void) hypot_mp (p, v, re, b, gdigs);
// im=ln(a/b).
    (void) div_mp (p, u, u, v, gdigs);
    (void) ln_mp (p, v, u, gdigs);
// re=atan(2r/(1-r^2-i^2)).
    (void) mul_mp (p, a, re, re, gdigs);
    (void) mul_mp (p, b, im, im, gdigs);
    (void) add_mp (p, a, a, b, gdigs);
    (void) sub_mp (p, u, c1, a, gdigs);
    if (IS_ZERO_MP (u)) {
      (void) mp_pi (p, u, MP_HALF_PI, gdigs);
    } else {
      int neg = MP_DIGIT (u, 1) < 0;
      (void) add_mp (p, a, re, re, gdigs);
      (void) div_mp (p, a, a, u, gdigs);
      (void) atan_mp (p, u, a, gdigs);
      if (neg) {
        (void) mp_pi (p, a, MP_PI, gdigs);
        if (MP_DIGIT (re, 1) < 0) {
          (void) sub_mp (p, u, u, a, gdigs);
        } else {
          (void) add_mp (p, u, u, a, gdigs);
        }
      }
    }
  }
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  (void) half_mp (p, r, r, digs);
  (void) half_mp (p, i, i, digs);
  A68_SP = pop_sp;
  return re;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX csinh

MP_T *csinh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// sinh (z) =  -i sin (iz)
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
  (void) csin_mp (p, u, v, gdigs);
  SET_MP_ZERO (s, gdigs);
  SET_MP_MINUS_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX ccosh

MP_T *ccosh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// cosh (z) =  cos (iz)
  SET_MP_ZERO (s, digs);
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
  (void) ccos_mp (p, u, v, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX ctanh

MP_T *ctanh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// tanh (z) =  -i tan (iz)
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
  (void) ctan_mp (p, u, v, gdigs);
  SET_MP_ZERO (u, gdigs);
  SET_MP_MINUS_ONE (v, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX casinh

MP_T *casinh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// asinh (z) =  i asin (-iz)
  SET_MP_MINUS_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
  (void) casin_mp (p, u, v, gdigs);
  SET_MP_ZERO (s, gdigs);
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX cacosh

MP_T *cacosh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// acosh (z) =  i * acos (z)
  (void) cacos_mp (p, u, v, gdigs);
  SET_MP_ZERO (s, gdigs);
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}

//! @brief PROC (LONG COMPLEX) LONG COMPLEX catanh

MP_T *catanh_mp (NODE_T * p, MP_T * r, MP_T * i, int digs)
{
  ADDR_T pop_sp = A68_SP;
  int gdigs = FUN_DIGITS (digs);
  MP_T *u = len_mp (p, r, digs, gdigs);
  MP_T *v = len_mp (p, i, digs, gdigs);
  MP_T *s = nil_mp (p, gdigs);
  MP_T *t = nil_mp (p, gdigs);
// atanh (z) =  i * atan (-iz)
  SET_MP_MINUS_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
  (void) catan_mp (p, u, v, gdigs);
  SET_MP_ZERO (s, gdigs);
  SET_MP_ONE (t, gdigs);
  (void) cmul_mp (p, u, v, s, t, gdigs);
//
  (void) shorten_mp (p, r, digs, u, gdigs);
  (void) shorten_mp (p, i, digs, v, gdigs);
  A68_SP = pop_sp;
  return r;
}
