//! @file a68g-double.h
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

#if !defined (__A68G_DOUBLE_H__)
#define __A68G_DOUBLE_H__

#if (A68_LEVEL >= 3)

#define MODCHK(p, m, c) (!(MODULAR_MATH (p) && (m == M_LONG_BITS)) && (c))


#if defined (HAVE_IEEE_754)
#define CHECK_DOUBLE_REAL(p, u) PRELUDE_ERROR (!finiteq (u), p, ERROR_INFINITE, M_LONG_REAL)
#define CHECK_DOUBLE_COMPLEX(p, u, v)\
  PRELUDE_ERROR (isinfq (u), p, ERROR_INFINITE, M_LONG_REAL);\
  PRELUDE_ERROR (isinfq (v), p, ERROR_INFINITE, M_LONG_REAL);
#else
#define CHECK_DOUBLE_REAL(p, u) {;}
#define CHECK_DOUBLE_COMPLEX(p, u, v) {;}
#endif

#define LONG_INT_BASE (9223372036854775808.0q)
#define HW(z) ((z).u[1])
#define LW(z) ((z).u[0])
#define D_NEG(d) ((HW(d) & D_SIGN) != 0)
#define D_LT(u, v) (HW (u) < HW (v) ? A68_TRUE : (HW (u) == HW (v) ? LW (u) < LW (v) : A68_FALSE))

#define DBLEQ(z) ((dble_16 (A68 (f_entry), (z))).f)

#define ABSQ(n) ((n) >= 0.0q ? (n) : -(n))

#define POP_LONG_COMPLEX(p, re, im) {\
  POP_OBJECT (p, im, A68_LONG_REAL);\
  POP_OBJECT (p, re, A68_LONG_REAL);\
  }

#define set_lw(z, k) {LW(z) = k; HW(z) = 0;}
#define set_hw(z, k) {LW(z) = 0; HW(z) = k;}
#define set_hwlw(z, h, l) {LW (z) = l; HW (z) = h;}
#define D_ZERO(z) (HW (z) == 0 && LW (z) == 0)

#define add_double(p, m, w, u, v)\
  {\
    QUAD_WORD_T _ww_;\
    LW (_ww_) = LW (u) + LW (v);\
    HW (_ww_) = HW (u) + HW (v);\
    PRELUDE_ERROR (MODCHK (p, m, HW (_ww_) < HW (v)), p, ERROR_MATH, (m));\
    if (LW (_ww_) < LW (v)) {\
      HW (_ww_)++;\
      PRELUDE_ERROR (MODCHK (p, m, HW (_ww_) < 1), p, ERROR_MATH, (m));\
    }\
    w = _ww_;\
  }

#define sub_double(p, m, w, u, v)\
  {\
    QUAD_WORD_T _ww_;\
    LW (_ww_) = LW (u) - LW (v);\
    HW (_ww_) = HW (u) - HW (v);\
    PRELUDE_ERROR (MODCHK (p, m, HW (_ww_) > HW (u)), p, ERROR_MATH, (m));\
    if (LW (_ww_) > LW (u)) {\
      PRELUDE_ERROR (MODCHK (p, m, HW (_ww_) == 0), p, ERROR_MATH, (m));\
      HW (_ww_)--;\
    }\
    w = _ww_;\
  }

static inline QUAD_WORD_T dble (DOUBLE_T x)
{
  QUAD_WORD_T w;
  w.f = x;
  return w;
}

static inline int sign_int_16 (QUAD_WORD_T w)
{
  if (D_NEG (w)) {
    return -1;
  } else if (D_ZERO (w)) {
    return 0;
  } else {
    return 1;
  }
}

static inline int sign_real_16 (QUAD_WORD_T w)
{
  if (w.f < 0.0q) {
    return -1;
  } else if (w.f == 0.0q) {
    return 0;
  } else {
    return 1;
  }
}

static inline QUAD_WORD_T inline abs_int_16 (QUAD_WORD_T z)
{
  QUAD_WORD_T w;
  LW (w) = LW (z);
  HW (w) = HW (z) & (~D_SIGN);
  return w;
}

static inline QUAD_WORD_T inline neg_int_16 (QUAD_WORD_T z)
{
  QUAD_WORD_T w;
  LW (w) = LW (z);
  if (D_NEG (z)) {
    HW (w) = HW (z) & (~D_SIGN);
  } else {
    HW (w) = HW (z) | D_SIGN;
  }
  return w;
}

extern int sign_int_16 (QUAD_WORD_T);
extern int sign_real_16 (QUAD_WORD_T);
extern int string_to_int_16 (NODE_T *, A68_LONG_INT *, char *);
extern DOUBLE_T a68_double_hypot (DOUBLE_T, DOUBLE_T);
extern DOUBLE_T a68_strtoq (char *, char **);
extern DOUBLE_T inverf_real_16 (DOUBLE_T);
extern QUAD_WORD_T abs_int_16 (QUAD_WORD_T);
extern QUAD_WORD_T bits_to_int_16 (NODE_T *, char *);
extern QUAD_WORD_T dble_16 (NODE_T *, REAL_T);
extern QUAD_WORD_T int_16_to_real_16 (NODE_T *, QUAD_WORD_T);
extern QUAD_WORD_T double_strtou (NODE_T *, char *);
extern QUAD_WORD_T double_udiv (NODE_T *, MOID_T *, QUAD_WORD_T, QUAD_WORD_T, int);
extern DOUBLE_T a68_dneginf (void);
extern DOUBLE_T a68_dposinf (void);
extern void deltagammainc_16 (DOUBLE_T *, DOUBLE_T *, DOUBLE_T, DOUBLE_T, DOUBLE_T, DOUBLE_T);

extern GPROC genie_infinity_real_16;
extern GPROC genie_minus_infinity_real_16;
extern GPROC genie_gamma_inc_g_real_16;
extern GPROC genie_gamma_inc_f_real_16;
extern GPROC genie_gamma_inc_h_real_16;
extern GPROC genie_gamma_inc_gf_real_16;
extern GPROC genie_abs_complex_32;
extern GPROC genie_abs_int_16;
extern GPROC genie_abs_real_16;
extern GPROC genie_acos_complex_32;
extern GPROC genie_acosdg_real_16;
extern GPROC genie_acosh_complex_32;
extern GPROC genie_acosh_real_16;
extern GPROC genie_acos_real_16;
extern GPROC genie_acotdg_real_16;
extern GPROC genie_acot_real_16;
extern GPROC genie_asec_real_16;
extern GPROC genie_acsc_real_16;
extern GPROC genie_add_complex_32;
extern GPROC genie_add_double_bits;
extern GPROC genie_add_int_16;
extern GPROC genie_add_real_16;
extern GPROC genie_add_real_16;
extern GPROC genie_and_double_bits;
extern GPROC genie_arg_complex_32;
extern GPROC genie_asin_complex_32;
extern GPROC genie_asindg_real_16;
extern GPROC genie_asindg_real_16;
extern GPROC genie_asinh_complex_32;
extern GPROC genie_asinh_real_16;
extern GPROC genie_asin_real_16;
extern GPROC genie_atan2dg_real_16;
extern GPROC genie_atan2_real_16;
extern GPROC genie_atan_complex_32;
extern GPROC genie_atandg_real_16;
extern GPROC genie_atanh_complex_32;
extern GPROC genie_atanh_real_16;
extern GPROC genie_atan_real_16;
extern GPROC genie_bin_int_16;
extern GPROC genie_clear_double_bits;
extern GPROC genie_conj_complex_32;
extern GPROC genie_cos_complex_32;
extern GPROC genie_cosdg_real_16;
extern GPROC genie_cosdg_real_16;
extern GPROC genie_cosh_complex_32;
extern GPROC genie_cosh_real_16;
extern GPROC genie_cospi_real_16;
extern GPROC genie_cos_real_16;
extern GPROC genie_cotdg_real_16;
extern GPROC genie_cotpi_real_16;
extern GPROC genie_cot_real_16;
extern GPROC genie_sec_real_16;
extern GPROC genie_csc_real_16;
extern GPROC genie_curt_real_16;
extern GPROC genie_divab_complex_32;
extern GPROC genie_divab_real_16;
extern GPROC genie_divab_real_16;
extern GPROC genie_div_complex_32;
extern GPROC genie_div_int_16;
extern GPROC genie_double_bits_pack;
extern GPROC genie_double_max_bits;
extern GPROC genie_double_max_int;
extern GPROC genie_double_max_real;
extern GPROC genie_double_min_real;
extern GPROC genie_double_small_real;
extern GPROC genie_double_zeroin;
extern GPROC genie_elem_double_bits;
extern GPROC genie_entier_real_16;
extern GPROC genie_eq_complex_32;
extern GPROC genie_eq_double_bits;
extern GPROC genie_eq_int_16;
extern GPROC genie_eq_int_16;
extern GPROC genie_eq_real_16;
extern GPROC genie_eq_real_16;
extern GPROC genie_eq_real_16;
extern GPROC genie_eq_real_16;
extern GPROC genie_erfc_real_16;
extern GPROC genie_erf_real_16;
extern GPROC genie_exp_complex_32;
extern GPROC genie_exp_real_16;
extern GPROC genie_gamma_real_16;
extern GPROC genie_ge_double_bits;
extern GPROC genie_ge_int_16;
extern GPROC genie_ge_int_16;
extern GPROC genie_ge_real_16;
extern GPROC genie_ge_real_16;
extern GPROC genie_ge_real_16;
extern GPROC genie_ge_real_16;
extern GPROC genie_gt_double_bits;
extern GPROC genie_gt_int_16;
extern GPROC genie_gt_int_16;
extern GPROC genie_gt_real_16;
extern GPROC genie_gt_real_16;
extern GPROC genie_gt_real_16;
extern GPROC genie_gt_real_16;
extern GPROC genie_i_complex_32;
extern GPROC genie_i_int_complex_32;
extern GPROC genie_im_complex_32;
extern GPROC genie_inverfc_real_16;
extern GPROC genie_inverf_real_16;
extern GPROC genie_le_double_bits;
extern GPROC genie_le_int_16;
extern GPROC genie_le_int_16;
extern GPROC genie_lengthen_bits_to_double_bits;
extern GPROC genie_lengthen_complex_32_to_long_mp_complex;
extern GPROC genie_lengthen_complex_to_complex_32;
extern GPROC genie_lengthen_int_16_to_mp;
extern GPROC genie_lengthen_int_to_int_16;
extern GPROC genie_lengthen_real_16_to_mp;
extern GPROC genie_lengthen_real_to_real_16;
extern GPROC genie_le_real_16;
extern GPROC genie_le_real_16;
extern GPROC genie_le_real_16;
extern GPROC genie_le_real_16;
extern GPROC genie_ln_complex_32;
extern GPROC genie_lngamma_real_16;
extern GPROC genie_ln_real_16;
extern GPROC genie_log_real_16;
extern GPROC genie_lt_double_bits;
extern GPROC genie_lt_int_16;
extern GPROC genie_lt_int_16;
extern GPROC genie_lt_real_16;
extern GPROC genie_lt_real_16;
extern GPROC genie_lt_real_16;
extern GPROC genie_lt_real_16;
extern GPROC genie_minusab_complex_32;
extern GPROC genie_minusab_int_16;
extern GPROC genie_minusab_int_16;
extern GPROC genie_minusab_real_16;
extern GPROC genie_minusab_real_16;
extern GPROC genie_minus_complex_32;
extern GPROC genie_minus_int_16;
extern GPROC genie_minus_real_16;
extern GPROC genie_modab_int_16;
extern GPROC genie_modab_int_16;
extern GPROC genie_mod_double_bits;
extern GPROC genie_mod_int_16;
extern GPROC genie_mul_complex_32;
extern GPROC genie_mul_int_16;
extern GPROC genie_mul_real_16;
extern GPROC genie_mul_real_16;
extern GPROC genie_ne_complex_32;
extern GPROC genie_ne_double_bits;
extern GPROC genie_ne_int_16;
extern GPROC genie_ne_int_16;
extern GPROC genie_ne_int_16;
extern GPROC genie_ne_int_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_ne_real_16;
extern GPROC genie_next_random_real_16;
extern GPROC genie_not_double_bits;
extern GPROC genie_odd_int_16;
extern GPROC genie_or_double_bits;
extern GPROC genie_overab_int_16;
extern GPROC genie_overab_int_16;
extern GPROC genie_over_double_bits;
extern GPROC genie_over_int_16;
extern GPROC genie_over_real_16;
extern GPROC genie_over_real_16;
extern GPROC genie_pi_double;
extern GPROC genie_plusab_complex_32;
extern GPROC genie_plusab_int_16;
extern GPROC genie_plusab_int_16;
extern GPROC genie_plusab_real_16;
extern GPROC genie_pow_complex_32_int;
extern GPROC genie_pow_int_16_int;
extern GPROC genie_pow_real_16;
extern GPROC genie_pow_real_16_int;
extern GPROC genie_re_complex_32;
extern GPROC genie_rol_double_bits;
extern GPROC genie_ror_double_bits;
extern GPROC genie_round_real_16;
extern GPROC genie_set_double_bits;
extern GPROC genie_shl_double_bits;
extern GPROC genie_shorten_complex_32_to_complex;
extern GPROC genie_shorten_double_bits_to_bits;
extern GPROC genie_shorten_long_int_to_int;
extern GPROC genie_shorten_long_mp_complex_to_complex_32;
extern GPROC genie_shorten_mp_to_int_16;
extern GPROC genie_shorten_mp_to_real_16;
extern GPROC genie_shorten_real_16_to_real;
extern GPROC genie_shr_double_bits;
extern GPROC genie_sign_int_16;
extern GPROC genie_sign_real_16;
extern GPROC genie_sin_complex_32;
extern GPROC genie_sindg_real_16;
extern GPROC genie_sinh_complex_32;
extern GPROC genie_sinh_real_16;
extern GPROC genie_sinpi_real_16;
extern GPROC genie_sin_real_16;
extern GPROC genie_sqrt_complex_32;
extern GPROC genie_sqrt_double;
extern GPROC genie_sqrt_real_16;
extern GPROC genie_sqrt_real_16;
extern GPROC genie_sub_complex_32;
extern GPROC genie_sub_double_bits;
extern GPROC genie_sub_int_16;
extern GPROC genie_sub_real_16;
extern GPROC genie_sub_real_16;
extern GPROC genie_tan_complex_32;
extern GPROC genie_tandg_real_16;
extern GPROC genie_tanh_complex_32;
extern GPROC genie_tanh_real_16;
extern GPROC genie_tanpi_real_16;
extern GPROC genie_tan_real_16;
extern GPROC genie_timesab_complex_32;
extern GPROC genie_timesab_int_16;
extern GPROC genie_timesab_int_16;
extern GPROC genie_timesab_real_16;
extern GPROC genie_timesab_real_16;
extern GPROC genie_times_double_bits;
extern GPROC genie_widen_int_16_to_real_16;
extern GPROC genie_xor_double_bits;
extern GPROC genie_beta_inc_cf_real_16;
extern GPROC genie_beta_real_16;
extern GPROC genie_ln_beta_real_16;
extern GPROC genie_gamma_inc_real_16;
extern GPROC genie_zero_int_16;

#endif

#endif
