/*!
\file mp.h
\brief contains multi-precision related definitions
**/

/*
This file is part of Algol68G - an Algol 68 interpreter.
Copyright (C) 2001-2009 J. Marcel van der Veer <algol68g@xs4all.nl>.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; either version 3 of the License, or (at your option) any later
version.

This program is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS FOR A
PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with 
this program. If not, see <http://www.gnu.org/licenses/>.
*/

#if ! defined A68G_MP_H
#define A68G_MP_H

/* Definitions for the multi-precision library. */

typedef double MP_DIGIT_T;

#define DEFAULT_MP_RADIX 	10000000
#define DEFAULT_DOUBLE_DIGITS	5

#define MP_RADIX      	DEFAULT_MP_RADIX
#define LOG_MP_BASE    	7
#define MP_BITS_RADIX	8388608 /* Max power of two smaller than MP_RADIX */
#define MP_BITS_BITS	23

/* 28-35 decimal digits for LONG REAL. */
#define LONG_MP_DIGITS 	DEFAULT_DOUBLE_DIGITS

#define MAX_REPR_INT   	9007199254740992.0	/* 2^53, max int in a double. */
#define MAX_MP_EXPONENT 142857	/* Arbitrary. Let M = MAX_REPR_INT then the
				   largest range is M / Log M / LOG_MP_BASE. */

#define MAX_MP_PRECISION 5000	/* If larger then library gets inefficient. */

extern int varying_mp_digits;

#define DOUBLE_ACCURACY (DBL_DIG - 1)

#define LONG_EXP_WIDTH       (EXP_WIDTH)
#define LONGLONG_EXP_WIDTH   (EXP_WIDTH)

#define LONG_WIDTH           (LONG_MP_DIGITS * LOG_MP_BASE)
#define LONGLONG_WIDTH       (varying_mp_digits * LOG_MP_BASE)

#define LONG_INT_WIDTH       (1 + LONG_WIDTH)
#define LONGLONG_INT_WIDTH   (1 + LONGLONG_WIDTH)

/* When changing L REAL_WIDTH mind that a mp number may not have more than
   1 + (MP_DIGITS - 1) * LOG_MP_BASE digits. Next definition is in accordance
   with REAL_WIDTH. */

#define LONG_REAL_WIDTH      ((LONG_MP_DIGITS - 1) * LOG_MP_BASE)
#define LONGLONG_REAL_WIDTH  ((varying_mp_digits - 1) * LOG_MP_BASE)

#define LOG2_10			3.321928094887362347870319430
#define MP_BITS_WIDTH(k)	((int) ceil((k) * LOG_MP_BASE * LOG2_10) - 1)
#define MP_BITS_WORDS(k)	((int) ceil ((double) MP_BITS_WIDTH (k) / (double) MP_BITS_BITS))

enum
{ MP_PI, MP_TWO_PI, MP_HALF_PI };

/* MP Macros. */

#define MP_STATUS(z) ((z)[0])
#define MP_EXPONENT(z) ((z)[1])
#define MP_DIGIT(z, n) ((z)[(n) + 1])

#define SIZE_MP(digits) ((2 + digits) * ALIGNED_SIZE_OF (MP_DIGIT_T))

#define IS_ZERO_MP(z) (MP_DIGIT (z, 1) == (MP_DIGIT_T) 0)

#define MOVE_MP(z, x, digits) {\
  MP_DIGIT_T *m_d = (z), *m_s = (x); int m_k = digits + 2;\
  while (m_k--) {*m_d++ = *m_s++;}\
  }

#define MOVE_DIGITS(z, x, digits) {\
  MP_DIGIT_T *m_d = (z), *m_s = (x); int m_k = digits;\
  while (m_k--) {*m_d++ = *m_s++;}\
  }

#define TEST_MP_INIT(p, z, m) {\
  if (! ((int) z[0] & INITIALISED_MASK)) {\
    diagnostic_node (A68_RUNTIME_ERROR, (p), ERROR_EMPTY_VALUE, (m));\
    exit_genie ((p), A68_RUNTIME_ERROR);\
  }}

#define CHECK_MP_EXPONENT(p, z) {\
  MP_DIGIT_T expo = fabs (MP_EXPONENT (z));\
  if (expo > MAX_MP_EXPONENT || (expo == MAX_MP_EXPONENT && ABS (MP_DIGIT (z, 1)) > 1.0)) {\
      errno = ERANGE;\
      diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_MP_OUT_OF_BOUNDS, NULL);\
      exit_genie (p, A68_RUNTIME_ERROR);\
  }}

#define SET_MP_ZERO(z, digits) {\
  MP_DIGIT_T *m_d = &MP_DIGIT ((z), 1); int m_k = digits;\
  MP_STATUS (z) = INITIALISED_MASK; MP_EXPONENT (z) = 0.0;\
  while (m_k--) {*m_d++ = 0.0;}\
  }

/* stack_mp: allocate temporary space in the evaluation stack. */

#define STACK_MP(dest, p, digits) {\
  ADDR_T stack_mp_sp = stack_pointer;\
  if ((stack_pointer += SIZE_MP (digits)) > expr_stack_limit) {\
    diagnostic_node (A68_RUNTIME_ERROR, p, ERROR_STACK_OVERFLOW);\
    exit_genie (p, A68_RUNTIME_ERROR);\
  }\
  dest = (MP_DIGIT_T *) STACK_ADDRESS (stack_mp_sp);\
}

/* External procedures. */

extern BOOL_T check_long_int (MP_DIGIT_T *);
extern BOOL_T check_longlong_int (MP_DIGIT_T *);
extern BOOL_T check_mp_int (MP_DIGIT_T *, MOID_T *);
extern MP_DIGIT_T *acos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *acosh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *add_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *asin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *asinh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atan2_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *atanh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cacos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *casin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *catan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ccos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cdiv_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cexp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cln_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cmul_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cos_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *cosh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *csin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *csqrt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ctan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *curt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *div_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *div_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *exp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *expm1_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *hyp_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *hypot_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *int_to_mp (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *lengthen_mp (NODE_T *, MP_DIGIT_T *, int, MP_DIGIT_T *, int);
extern MP_DIGIT_T *ln_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *log_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mod_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mp_pi (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *mp_ten_up (NODE_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *mul_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *mul_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *over_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *over_mp_digit (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T, int);
extern MP_DIGIT_T *pack_mp_bits (NODE_T *, MP_DIGIT_T *, unsigned *, MOID_T *);
extern MP_DIGIT_T *pow_mp_int (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int, int);
extern MP_DIGIT_T *real_to_mp (NODE_T *, MP_DIGIT_T *, double, int);
extern MP_DIGIT_T *set_mp_short (MP_DIGIT_T *, MP_DIGIT_T, int, int);
extern MP_DIGIT_T *shorten_mp (NODE_T *, MP_DIGIT_T *, int, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sin_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sinh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *sqrt_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *string_to_mp (NODE_T *, MP_DIGIT_T *, char *, int);
extern MP_DIGIT_T *sub_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *tan_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *tanh_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);
extern MP_DIGIT_T *unsigned_to_mp (NODE_T *, MP_DIGIT_T *, unsigned, int);
extern char *long_sub_fixed (NODE_T *, MP_DIGIT_T *, int, int, int);
extern char *long_sub_whole (NODE_T *, MP_DIGIT_T *, int, int);
extern double mp_to_real (NODE_T *, MP_DIGIT_T *, int);
extern int get_mp_bits_width (MOID_T *);
extern int get_mp_bits_words (MOID_T *);
extern int get_mp_digits (MOID_T *);
extern int get_mp_size (MOID_T *);
extern int int_to_mp_digits (int);
extern int long_mp_digits (void);
extern int longlong_mp_digits (void);
extern int mp_to_int (NODE_T *, MP_DIGIT_T *, int);
extern size_t size_long_mp (void);
extern size_t size_longlong_mp (void);
extern unsigned *stack_mp_bits (NODE_T *, MP_DIGIT_T *, MOID_T *);
extern unsigned mp_to_unsigned (NODE_T *, MP_DIGIT_T *, int);
extern void check_long_bits_value (NODE_T *, MP_DIGIT_T *, MOID_T *);
extern void long_standardise (NODE_T *, MP_DIGIT_T *, int, int, int, int *);
extern void raw_write_mp (char *, MP_DIGIT_T *, int);
extern void set_longlong_mp_digits (int);
extern void trunc_mp (NODE_T *, MP_DIGIT_T *, MP_DIGIT_T *, int);

#endif
