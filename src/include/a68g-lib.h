//! @file a68g-lib.h
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

#if !defined __A68G_LIB_H__
#define __A68G_LIB_H__

#define MATH_EPSILON DBL_EPSILON

#define A68_INVALID(c)\
  if (c) {\
    errno = EDOM;\
    return 0;\
  }

#define A68_OVERFLOW(c)\
  if (c) {\
    errno = ERANGE;\
    return 0;\
  }

#define A68_MAX_FAC 170

extern INT_T a68_add_int (INT_T, INT_T);
extern INT_T a68_mod_int (INT_T, INT_T);
extern INT_T a68_mul_int (INT_T, INT_T);
extern INT_T a68_m_up_n (INT_T, INT_T);
extern INT_T a68_over_int (INT_T, INT_T);
extern INT_T a68_round (REAL_T);
extern INT_T a68_sub_int (INT_T, INT_T);
extern REAL_T a68_abs (REAL_T);
extern REAL_T a68_acosdg (REAL_T);
extern REAL_T a68_acosh (REAL_T);
extern REAL_T a68_acotdg (REAL_T);
extern REAL_T a68_acot (REAL_T);
extern REAL_T a68_acsc (REAL_T);
extern REAL_T a68_asec (REAL_T);
extern REAL_T a68_asindg (REAL_T);
extern REAL_T a68_asinh (REAL_T);
extern REAL_T a68_atan2 (REAL_T, REAL_T);
extern REAL_T a68_atandg (REAL_T);
extern REAL_T a68_atanh (REAL_T);
extern REAL_T a68_beta (REAL_T, REAL_T);
extern REAL_T a68_choose (INT_T, INT_T);
extern REAL_T a68_cosdg (REAL_T);
extern REAL_T a68_cospi (REAL_T);
extern REAL_T a68_cotdg (REAL_T);
extern REAL_T a68_cotpi (REAL_T);
extern REAL_T a68_cot (REAL_T);
extern REAL_T a68_csc (REAL_T);
extern REAL_T a68_sec (REAL_T);
extern REAL_T a68_div_int (INT_T, INT_T);
extern REAL_T a68_exp (REAL_T);
extern REAL_T a68_fact (INT_T);
extern REAL_T a68_fdiv (REAL_T, REAL_T);
extern REAL_T a68_hypot (REAL_T, REAL_T);
extern REAL_T a68_int (REAL_T);
extern REAL_T a68_inverfc (REAL_T);
extern REAL_T a68_inverf (REAL_T);
extern REAL_T a68_ln1p (REAL_T);
extern REAL_T a68_ln1p (REAL_T);
extern REAL_T a68_ln_beta (REAL_T, REAL_T);
extern REAL_T a68_ln_choose (INT_T, INT_T);
extern REAL_T a68_ln_fact (INT_T);
extern REAL_T a68_ln (REAL_T);
extern REAL_T a68_max (REAL_T, REAL_T);
extern REAL_T a68_min (REAL_T, REAL_T);
extern REAL_T a68_nan (void);
extern REAL_T a68_neginf (void);
extern REAL_T a68_posinf (void);
extern REAL_T a68_psi (REAL_T);
extern REAL_T a68_sign (REAL_T);
extern REAL_T a68_sindg (REAL_T);
extern REAL_T a68_sinpi (REAL_T);
extern REAL_T a68_tandg (REAL_T);
extern REAL_T a68_tanpi (REAL_T);
extern REAL_T a68_x_up_n (REAL_T, INT_T);
extern REAL_T a68_x_up_y (REAL_T, REAL_T);
extern REAL_T a68_beta_inc (REAL_T, REAL_T, REAL_T);
extern DOUBLE_T a68_beta_inc_double_real (DOUBLE_T, DOUBLE_T, DOUBLE_T);
extern DOUBLE_T cot_double_real (DOUBLE_T);
extern DOUBLE_T csc_double_real (DOUBLE_T);
extern DOUBLE_T sec_double_real (DOUBLE_T);
extern DOUBLE_T acot_double_real (DOUBLE_T);
extern DOUBLE_T acsc_double_real (DOUBLE_T);
extern DOUBLE_T asec_double_real (DOUBLE_T);
extern DOUBLE_T sindg_double_real (DOUBLE_T);
extern DOUBLE_T cosdg_double_real (DOUBLE_T);
extern DOUBLE_T tandg_double_real (DOUBLE_T);
extern DOUBLE_T asindg_double_real (DOUBLE_T);
extern DOUBLE_T acosdg_double_real (DOUBLE_T);
extern DOUBLE_T atandg_double_real (DOUBLE_T);
extern DOUBLE_T cotdg_double_real (DOUBLE_T);
extern DOUBLE_T acotdg_double_real (DOUBLE_T);
extern DOUBLE_T sinpi_double_real (DOUBLE_T); 
extern DOUBLE_T cospi_double_real (DOUBLE_T); 
extern DOUBLE_T tanpi_double_real (DOUBLE_T);
extern DOUBLE_T cotpi_double_real (DOUBLE_T);

#endif
