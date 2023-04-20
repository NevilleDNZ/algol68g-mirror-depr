//! @file a68g-optimiser.h
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

#if !defined (__A68G_OPTIMISER_H__)
#define __A68G_OPTIMISER_H__

extern BOOL_T constant_unit (NODE_T *);
extern BOOL_T folder_mode (MOID_T *);
extern void build_script (void);
extern void compiler (FILE_T);
extern void load_script (void);
extern void push_unit (NODE_T *);
extern void rewrite_script_source (void);

// Library for code generator

extern INT_T a68_add_int (INT_T, INT_T);
extern INT_T a68_sub_int (INT_T, INT_T);
extern INT_T a68_mul_int (INT_T, INT_T);
extern INT_T a68_over_int (INT_T, INT_T);
extern INT_T a68_mod_int (INT_T, INT_T);
extern REAL_T a68_div_int (INT_T, INT_T);

extern void a68_ln_complex (A68_REAL *, A68_REAL *);
extern void a68_sqrt_complex (A68_REAL *, A68_REAL *);
extern void a68_sin_complex (A68_REAL *, A68_REAL *);
extern void a68_cos_complex (A68_REAL *, A68_REAL *);
extern void a68_tan_complex (A68_REAL *, A68_REAL *);
extern void a68_asin_complex (A68_REAL *, A68_REAL *);
extern void a68_acos_complex (A68_REAL *, A68_REAL *);
extern void a68_atan_complex (A68_REAL *, A68_REAL *);
extern void a68_sinh_complex (A68_REAL *, A68_REAL *);
extern void a68_cosh_complex (A68_REAL *, A68_REAL *);
extern void a68_tanh_complex (A68_REAL *, A68_REAL *);
extern void a68_asinh_complex (A68_REAL *, A68_REAL *);
extern void a68_acosh_complex (A68_REAL *, A68_REAL *);
extern void a68_atanh_complex (A68_REAL *, A68_REAL *);

// Operators that are inlined in compiled code

#define a68_eq_complex(x, y) (RE (x) == RE (y) && IM (x) == IM (y))
#define a68_ne_complex(x, y) (! a68_eq_complex (x, y))
#define a68_plusab_int(i, j) (VALUE ((A68_INT *) ADDRESS (i)) += (j), (i))
#define a68_minusab_int(i, j) (VALUE ((A68_INT *) ADDRESS (i)) -= (j), (i))
#define a68_timesab_int(i, j) (VALUE ((A68_INT *) ADDRESS (i)) *= (j), (i))
#define a68_overab_int(i, j) (VALUE ((A68_INT *) ADDRESS (i)) /= (j), (i))
#define a68_entier(x) ((int) floor (x))
#define a68_plusab_real(i, j) (VALUE ((A68_REAL *) ADDRESS (i)) += (j), (i))
#define a68_minusab_real(i, j) (VALUE ((A68_REAL *) ADDRESS (i)) -= (j), (i))
#define a68_timesab_real(i, j) (VALUE ((A68_REAL *) ADDRESS (i)) *= (j), (i))
#define a68_divab_real(i, j) (VALUE ((A68_REAL *) ADDRESS (i)) /= (j), (i))
#define a68_re_complex(z) (RE (z))
#define a68_im_complex(z) (IM (z))
#define a68_abs_complex(z) a68_hypot (RE (z), IM (z))
#define a68_arg_complex(z) atan2 (IM (z), RE (z))

#define a68_i_complex(z, re, im) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = re;\
  IM (z) = im;}

#define a68_minus_complex(z, x) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = -RE (x);\
  IM (z) = -IM (x);}

#define a68_conj_complex(z, x) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = RE (x);\
  IM (z) = -IM (x);}

#define a68_add_complex(z, x, y) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = RE (x) + RE (y);\
  IM (z) = IM (x) + IM (y);}

#define a68_sub_complex(z, x, y) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = RE (x) - RE (y);\
  IM (z) = IM (x) - IM (y);}

#define a68_mul_complex(z, x, y) {\
  STATUS_RE (z) = INIT_MASK;\
  STATUS_IM (z) = INIT_MASK;\
  RE (z) = RE (x) * RE (y) - IM (x) * IM (y);\
  IM (z) = IM (x) * RE (y) + RE (x) * IM (y);}

#endif
