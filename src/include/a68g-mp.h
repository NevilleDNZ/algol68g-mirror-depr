//! @file a68g-mp.h
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
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

#if !defined (__A68G_MP_H__)
#define __A68G_MP_H__

// A68G's multiprecision algorithms are not suited for more than a few hundred
// digits. This is however sufficient for most practical MP applications.

#define MP_MAX_DECIMALS 250

#define MP_STATUS(z) ((z)[0])
#define MP_EXPONENT(z) ((z)[1])
#define MP_DIGIT(z, n) ((z)[(n) + 1])
#define MP_SIGN(z) (SIGN (MP_DIGIT (z, 1)))
#define LEN_MP(digs) (2 + digs)
#define SIZE_MP(digs) A68_ALIGN (LEN_MP (digs) * sizeof (MP_T))
#define IS_ZERO_MP(z) (MP_DIGIT (z, 1) == (MP_T) 0)

#define PLUS_INF_MP(u) ((UNSIGNED_T) MP_STATUS (u) & PLUS_INF_MASK)
#define MINUS_INF_MP(u) ((UNSIGNED_T) MP_STATUS (u) & MINUS_INF_MASK)
#define INF_MP(u) (PLUS_INF_MP (u) || MINUS_INF_MP (u))
#define CHECK_LONG_REAL(p, u, moid) PRELUDE_ERROR (INF_MP (u), p, ERROR_INFINITE, moid)

static inline MP_T *set_mp (MP_T * z, MP_T x, INT_T expo, int digs)
{
  memset (z, 0, SIZE_MP (digs));
  MP_STATUS (z) = (MP_T) INIT_MASK;
  MP_DIGIT (z, 1) = x;
  MP_EXPONENT (z) = (MP_T) expo;
  return z;
}

static inline MP_T *move_mp (MP_T *z, MP_T *x, int N) 
{
  MP_T *y = z;
  N += 2;
  while (N--) {
    *z++ = *x++;
  }
  return y;
}

static inline MP_T *move_mp_part (MP_T *z, MP_T *x, int N) 
{
  MP_T *y = z;
  while (N--) {
    *z++ = *x++;
  }
  return y;
}

static inline void check_mp_exp (NODE_T *p, MP_T *z) 
{
  MP_T expo = (MP_EXPONENT (z) < 0 ? -MP_EXPONENT (z) : MP_EXPONENT (z));
  if (expo > MAX_MP_EXPONENT || (expo == MAX_MP_EXPONENT && ABS (MP_DIGIT (z, 1)) > (MP_T) 1)) {
    errno = EDOM;
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_MP_OUT_OF_BOUNDS, NULL);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
}

static inline MP_T *mp_one (int digs)
{
  if (digs > A68_MP (mp_one_size)) {
    if (A68_MP (mp_one) != (MP_T *) NULL) {
      a68_free (A68_MP (mp_one));
    }
    A68_MP (mp_one) = (MP_T *) get_heap_space (SIZE_MP (digs));
    set_mp (A68_MP (mp_one), 1, 0, digs);
  }
  return A68_MP (mp_one);
}

static inline MP_T *lit_mp (NODE_T *p, MP_T u, INT_T expo, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if ((A68_SP += SIZE_MP (digs)) > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_T *z = (MP_T *) STACK_ADDRESS (pop_sp);
  (void) set_mp (z, u, expo, digs);
  return z;
}

static inline MP_T *nil_mp (NODE_T *p, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if ((A68_SP += SIZE_MP (digs)) > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_T *z = (MP_T *) STACK_ADDRESS (pop_sp);
  (void) set_mp (z, 0, 0, digs);
  return z;
}

static inline MP_T *empty_mp (NODE_T *p, int digs)
{
  ADDR_T pop_sp = A68_SP;
  if ((A68_SP += SIZE_MP (digs)) > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  return (MP_T *) STACK_ADDRESS (pop_sp);
}

extern MP_T *lengthen_mp (NODE_T *, MP_T *, int, MP_T *, int);

static inline MP_T *len_mp (NODE_T *p, MP_T *u, int digs, int gdigs)
{
  ADDR_T pop_sp = A68_SP;
  if ((A68_SP += SIZE_MP (gdigs)) > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_T *z = (MP_T *) STACK_ADDRESS (pop_sp);
  for (int k = 1; k <= digs; k++) {
    MP_DIGIT (z, k) = MP_DIGIT (u, k);
  }
  for (int k = digs + 1; k <= gdigs; k++) {
    MP_DIGIT (z, k) = (MP_T) 0;
  }
  MP_EXPONENT (z) = MP_EXPONENT (u);
  MP_STATUS (z) = MP_STATUS (u);
  return z;
}

static inline MP_T *cut_mp (NODE_T *p, MP_T *u, int digs, int gdigs)
{
  ADDR_T pop_sp = A68_SP;
  ASSERT (digs > gdigs);
  BOOL_T neg = MP_DIGIT (u, 1) < 0;
  if ((A68_SP += SIZE_MP (gdigs)) > A68 (expr_stack_limit)) {
    diagnostic (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);
    extern void exit_genie (NODE_T *, int);
    exit_genie (p, A68_RUNTIME_ERROR);
  }
  MP_T *z = (MP_T *) STACK_ADDRESS (pop_sp);
  for (int k = 1; k <= gdigs; k++) {
    MP_DIGIT (z, k) = MP_DIGIT (u, k);
  }
  if (neg) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  if (MP_DIGIT (u, gdigs + 1) >= MP_RADIX / 2) {
    MP_DIGIT (z, gdigs) += 1;
    for (int k = digs; k >= 2 && MP_DIGIT (z, k) == MP_RADIX; k --) {
      MP_DIGIT (z, k) = 0;
      MP_DIGIT (z, k - 1) ++;
    }
  }
  if (neg) {
    MP_DIGIT (z, 1) = -MP_DIGIT (z, 1);
  }
  MP_EXPONENT (z) = MP_EXPONENT (u);
  MP_STATUS (z) = MP_STATUS (u);
  return z;
}

//! @brief Length in bytes of a long mp number.

static inline size_t size_mp (void)
{
  return (size_t) SIZE_MP (LONG_MP_DIGITS);
}

//! @brief Length in digits of a long mp number.

static inline int mp_digits (void)
{
  return LONG_MP_DIGITS;
}

//! @brief Length in bytes of a long long mp number.

static inline size_t size_long_mp (void)
{
  return (size_t) (SIZE_MP (A68_MP (varying_mp_digits)));
}

//! @brief digits in a long mp number.

static inline int long_mp_digits (void)
{
  return A68_MP (varying_mp_digits);
}

#define SET_MP_ZERO(z, digits)\
  (void) set_mp ((z), 0, 0, digits);

#define SET_MP_ONE(z, digits)\
  (void) set_mp ((z), (MP_T) 1, 0, digits);

#define SET_MP_MINUS_ONE(z, digits)\
  (void) set_mp ((z), (MP_T) -1, 0, digits);

#define SET_MP_HALF(z, digits)\
  (void) set_mp ((z), (MP_T) (MP_RADIX / 2), -1, digits);

#define SET_MP_MINUS_HALF(z, digits)\
  (void) set_mp ((z), (MP_T) -(MP_RADIX / 2), -1, digits);

#define SET_MP_QUART(z, digits)\
  (void) set_mp ((z), (MP_T) (MP_RADIX / 4), -1, digits);

enum {MP_SQRT_PI, MP_PI, MP_LN_PI, MP_SQRT_TWO_PI, MP_TWO_PI, MP_HALF_PI, MP_180_OVER_PI, MP_PI_OVER_180};

// If MP_DOUBLE_PRECISION is defined functions are evaluated in double precision.

#undef MP_DOUBLE_PRECISION

#define MINIMUM(x, y) ((x) < (y) ? (x) : (y))

// GUARD_DIGITS: number of guard digits.

#if defined (MP_DOUBLE_PRECISION)
#define GUARD_DIGITS(digits) (digits)
#else
#define GUARD_DIGITS(digits) (2)
#endif

#define FUN_DIGITS(n) ((n) + GUARD_DIGITS (n))

// External multi-precision procedures

extern BOOL_T check_mp_int (MP_T *, MOID_T *);
extern BOOL_T is_int_mp (NODE_T *p, MP_T *z, int digits);
extern BOOL_T same_mp (NODE_T *, MP_T *, MP_T *, int);
extern int long_mp_digits (void);
extern INT_T mp_to_int (NODE_T *, MP_T *, int);
extern int width_to_mp_digits (int);
extern MP_T *abs_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acosdg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acosh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acos_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acotdg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acot_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *acsc_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *add_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *align_mp (MP_T *, INT_T *, int);
extern MP_T *asec_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *asindg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *asinh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *asin_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *atan2dg_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *atan2_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *atandg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *atanh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *atan_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *beta_inc_mp (NODE_T *, MP_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *beta_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *cacosh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cacos_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *casinh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *casin_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *catanh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *catan_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *ccosh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *ccos_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cdiv_mp (NODE_T *, MP_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *cexp_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cln_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cmul_mp (NODE_T *, MP_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *cosdg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cosh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cos_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cospi_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cotdg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cot_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *cotpi_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *csc_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *csinh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *csin_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *csqrt_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *ctanh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *ctan_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *curt_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *div_mp_digit (NODE_T *, MP_T *, MP_T *, MP_T, int);
extern MP_T *div_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *entier_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *erfc_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *erf_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *expm1_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *exp_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *floor_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *gamma_inc_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *gamma_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *half_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *hyp_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *hypot_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *int_to_mp (NODE_T *, MP_T *, INT_T, int);
extern MP_T *inverfc_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *inverf_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *lnbeta_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *lngamma_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *ln_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *log_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *minus_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *minus_one_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *mod_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *mp_ln_10 (NODE_T *, MP_T *, int);
extern MP_T *mp_ln_scale (NODE_T *, MP_T *, int);
extern MP_T *mp_pi (NODE_T *, MP_T *, int, int);
extern MP_T *ten_up_mp (NODE_T *, MP_T *, int, int);
extern MP_T *mul_mp_digit (NODE_T *, MP_T *, MP_T *, MP_T, int);
extern MP_T *mul_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *one_minus_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *over_mp_digit (NODE_T *, MP_T *, MP_T *, MP_T, int);
extern MP_T *over_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *plus_one_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *pow_mp_int (NODE_T *, MP_T *, MP_T *, INT_T, int);
extern MP_T *pow_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *real_to_mp (NODE_T *, MP_T *, REAL_T, int);
extern MP_T *rec_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *round_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *sec_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *set_mp (MP_T *, MP_T, INT_T, int);
extern MP_T *shorten_mp (NODE_T *, MP_T *, int, MP_T *, int);
extern MP_T *sindg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *sinh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *sin_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *sinpi_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *sqrt_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *strtomp (NODE_T *, MP_T *, char *, int);
extern MP_T *sub_mp (NODE_T *, MP_T *, MP_T *, MP_T *, int);
extern MP_T *tandg_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *tanh_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *tan_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *tanpi_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *tenth_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *trunc_mp (NODE_T *, MP_T *, MP_T *, int);
extern MP_T *unsigned_to_mp (NODE_T *, MP_T *, UNSIGNED_T, int);
extern REAL_T mp_to_real (NODE_T *, MP_T *, int);
extern void eq_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void ge_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void genie_pi_mp (NODE_T *);
extern void gt_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void le_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void lt_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void ne_mp (NODE_T *, A68_BOOL *, MP_T *, MP_T *, int);
extern void raw_write_mp (char *, MP_T *, int);
extern void set_long_mp_digits (int);
extern void test_long_int_range (NODE_T *, MP_T *, MOID_T *);

extern GPROC genie_infinity_mp;
extern GPROC genie_minus_infinity_mp;
extern GPROC genie_beta_inc_mp;
extern GPROC genie_gamma_inc_mp;
extern GPROC genie_gamma_inc_f_mp;
extern GPROC genie_gamma_inc_g_mp;
extern GPROC genie_gamma_inc_h_mp;
extern GPROC genie_gamma_inc_gf_mp;
extern GPROC genie_abs_mp;
extern GPROC genie_abs_mp_complex;
extern GPROC genie_acosdg_mp;
extern GPROC genie_acosdg_mp;
extern GPROC genie_acosh_mp;
extern GPROC genie_acosh_mp_complex;
extern GPROC genie_acos_mp;
extern GPROC genie_acos_mp_complex;
extern GPROC genie_acotdg_mp;
extern GPROC genie_acot_mp;
extern GPROC genie_asec_mp;
extern GPROC genie_acsc_mp;
extern GPROC genie_add_mp;
extern GPROC genie_add_mp_complex;
extern GPROC genie_and_mp;
extern GPROC genie_arg_mp_complex;
extern GPROC genie_asindg_mp;
extern GPROC genie_asindg_mp;
extern GPROC genie_asinh_mp;
extern GPROC genie_asinh_mp_complex;
extern GPROC genie_asin_mp;
extern GPROC genie_asin_mp_complex;
extern GPROC genie_atan2_mp;
extern GPROC genie_atandg_mp;
extern GPROC genie_atan2dg_mp;
extern GPROC genie_atanh_mp;
extern GPROC genie_atanh_mp_complex;
extern GPROC genie_atan_mp;
extern GPROC genie_atan_mp_complex;
extern GPROC genie_bin_mp;
extern GPROC genie_clear_long_mp_bits;
extern GPROC genie_conj_mp_complex;
extern GPROC genie_cosdg_mp;
extern GPROC genie_cosh_mp;
extern GPROC genie_cosh_mp_complex;
extern GPROC genie_cos_mp;
extern GPROC genie_cos_mp_complex;
extern GPROC genie_cospi_mp;
extern GPROC genie_cotdg_mp;
extern GPROC genie_cot_mp;
extern GPROC genie_sec_mp;
extern GPROC genie_csc_mp;
extern GPROC genie_cotpi_mp;
extern GPROC genie_curt_mp;
extern GPROC genie_divab_mp;
extern GPROC genie_divab_mp_complex;
extern GPROC genie_div_mp;
extern GPROC genie_div_mp_complex;
extern GPROC genie_elem_long_mp_bits;
extern GPROC genie_elem_long_mp_bits;
extern GPROC genie_entier_mp;
extern GPROC genie_eq_mp;
extern GPROC genie_eq_mp_complex;
extern GPROC genie_erfc_mp;
extern GPROC genie_erf_mp;
extern GPROC genie_exp_mp;
extern GPROC genie_exp_mp_complex;
extern GPROC genie_gamma_mp;
extern GPROC genie_lngamma_mp;
extern GPROC genie_beta_mp;
extern GPROC genie_lnbeta_mp;
extern GPROC genie_ge_mp;
extern GPROC genie_get_long_mp_bits;
extern GPROC genie_get_long_mp_complex;
extern GPROC genie_get_long_mp_int;
extern GPROC genie_get_long_mp_real;
extern GPROC genie_get_mp_complex;
extern GPROC genie_gt_mp;
extern GPROC genie_im_mp_complex;
extern GPROC genie_inverfc_mp;
extern GPROC genie_inverf_mp;
extern GPROC genie_le_mp;
extern GPROC genie_lengthen_complex_to_mp_complex;
extern GPROC genie_lengthen_int_to_mp;
extern GPROC genie_lengthen_mp_complex_to_long_mp_complex;
extern GPROC genie_lengthen_mp_to_long_mp;
extern GPROC genie_lengthen_real_to_mp;
extern GPROC genie_lengthen_unsigned_to_mp;
extern GPROC genie_ln_mp;
extern GPROC genie_ln_mp_complex;
extern GPROC genie_log_mp;
extern GPROC genie_long_mp_bits_width;
extern GPROC genie_long_mp_exp_width;
extern GPROC genie_long_mp_int_width;
extern GPROC genie_long_mp_max_bits;
extern GPROC genie_long_mp_max_int;
extern GPROC genie_long_mp_max_real;
extern GPROC genie_long_mp_min_real;
extern GPROC genie_long_mp_real_width;
extern GPROC genie_long_mp_small_real;
extern GPROC genie_lt_mp;
extern GPROC genie_minusab_mp;
extern GPROC genie_minusab_mp_complex;
extern GPROC genie_minus_mp;
extern GPROC genie_minus_mp_complex;
extern GPROC genie_modab_mp;
extern GPROC genie_mod_mp;
extern GPROC genie_mul_mp;
extern GPROC genie_mul_mp_complex;
extern GPROC genie_ne_mp;
extern GPROC genie_ne_mp_complex;
extern GPROC genie_not_mp;
extern GPROC genie_odd_mp;
extern GPROC genie_or_mp;
extern GPROC genie_overab_mp;
extern GPROC genie_over_mp;
extern GPROC genie_pi_mp;
extern GPROC genie_plusab_mp;
extern GPROC genie_plusab_mp_complex;
extern GPROC genie_pow_mp;
extern GPROC genie_pow_mp_complex_int;
extern GPROC genie_pow_mp_int;
extern GPROC genie_pow_mp_int_int;
extern GPROC genie_print_long_mp_bits;
extern GPROC genie_print_long_mp_complex;
extern GPROC genie_print_long_mp_int;
extern GPROC genie_print_long_mp_real;
extern GPROC genie_print_mp_complex;
extern GPROC genie_put_long_mp_bits;
extern GPROC genie_put_long_mp_complex;
extern GPROC genie_put_long_mp_int;
extern GPROC genie_put_long_mp_real;
extern GPROC genie_put_mp_complex;
extern GPROC genie_read_long_mp_bits;
extern GPROC genie_read_long_mp_complex;
extern GPROC genie_read_long_mp_int;
extern GPROC genie_read_long_mp_real;
extern GPROC genie_read_mp_complex;
extern GPROC genie_re_mp_complex;
extern GPROC genie_round_mp;
extern GPROC genie_set_long_mp_bits;
extern GPROC genie_shl_mp;
extern GPROC genie_shorten_long_mp_complex_to_mp_complex;
extern GPROC genie_shorten_long_mp_to_mp;
extern GPROC genie_shorten_mp_complex_to_complex;
extern GPROC genie_shorten_mp_to_bits;
extern GPROC genie_shorten_mp_to_int;
extern GPROC genie_shorten_mp_to_real;
extern GPROC genie_shr_mp;
extern GPROC genie_sign_mp;
extern GPROC genie_sindg_mp;
extern GPROC genie_sinh_mp;
extern GPROC genie_sinh_mp_complex;
extern GPROC genie_sin_mp;
extern GPROC genie_sin_mp_complex;
extern GPROC genie_sinpi_mp;
extern GPROC genie_sqrt_mp;
extern GPROC genie_sqrt_mp_complex;
extern GPROC genie_sub_mp;
extern GPROC genie_sub_mp_complex;
extern GPROC genie_tandg_mp;
extern GPROC genie_tanh_mp;
extern GPROC genie_tanh_mp_complex;
extern GPROC genie_tan_mp;
extern GPROC genie_tan_mp_complex;
extern GPROC genie_tanpi_mp;
extern GPROC genie_timesab_mp;
extern GPROC genie_timesab_mp_complex;
extern GPROC genie_xor_mp;

#if defined (HAVE_GNU_MPFR)
extern GPROC genie_beta_inc_mpfr;
extern GPROC genie_ln_beta_mpfr;
extern GPROC genie_beta_mpfr;
extern GPROC genie_gamma_inc_mpfr;
extern GPROC genie_gamma_inc_real_mpfr;
extern GPROC genie_gamma_inc_real_16_mpfr;
extern GPROC genie_gamma_mpfr;
extern GPROC genie_lngamma_mpfr;
extern GPROC genie_mpfr_erfc_mp;
extern GPROC genie_mpfr_erf_mp;
extern GPROC genie_mpfr_inverfc_mp;
extern GPROC genie_mpfr_inverf_mp;
extern GPROC genie_mpfr_mp;
extern size_t mpfr_digits (void);
#endif

#if (A68_LEVEL <= 2)
extern int get_mp_bits_width (MOID_T *);
extern int get_mp_bits_words (MOID_T *);
extern MP_BITS_T *stack_mp_bits (NODE_T *, MP_T *, MOID_T *);
#endif

#endif
