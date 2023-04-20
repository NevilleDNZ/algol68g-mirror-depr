//! @file a68g-double.h
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

#if !defined (__A68G_QUAD_H__)
#define __A68G_QUAD_H__

#if (A68_LEVEL >= 3)

#if defined (BUILD_WIN32)
#define __BYTE_ORDER __LITTLE_ENDIAN // WIN32 runs on Intel.
#else
#include <endian.h>
#endif

#define unt_2 uint16_t
#define FLT128_LEN 	 7	// Do NOT change this!
#define FLT256_LEN 	 15	// Do NOT change this!
#define HPA_VERSION 	 "1.7 A68G"
#define FLT256_MANT_DIG (FLT256_LEN * 16)
#define QUAD_DIGITS MANT_DIGS (FLT256_MANT_DIG)

typedef unt_2 REAL16[8];
typedef unt_2 REAL32[FLT256_LEN + 1];
typedef struct QUAD_T QUAD_T;

struct QUAD_T
{
  REAL32 value;
};

#define QV(z) (z.value)

#define FLT256_DIG 62
#define MAX_FLT256_DIG (FLT256_DIG + 6)
#define M_LOG10_2 0.30102999566398119521373889472449q

#define QUAD_RTE(where, err) abend ((char *) err, (char *) where, __FILE__, __LINE__)

#define _add_quad_real_(a, b) add_quad_real (a, b, 0)
#define _sub_quad_real_(a, b) add_quad_real (a, b, 1)

#define CHECK_QUAD_REAL(p, u) PRELUDE_ERROR (isMinf_quad_real (u) || isPinf_quad_real (u), p, ERROR_INFINITE, M_LONG_LONG_REAL)

#define CHECK_QUAD_COMPLEX(p, u, v)\
  CHECK_QUAD_REAL (p, u);\
  CHECK_QUAD_REAL (p, v); 

// Shorthands to improve code legibility.

#define int_2 int16_t


#if defined (QUAD_REAL_ERR_IGN)
  #define sigerr_quad_real(errcond, errcode, where) 0
#else
  #define QUAD_REAL_EDIV    1
  #define QUAD_REAL_EDOM    2
  #define QUAD_REAL_EBADEXP 3
  #define QUAD_REAL_FPOFLOW 4
  #define QUAD_REAL_NERR    4
  #define QUAD_REAL_EINV    (QUAD_REAL_NERR + 1)
  int sigerr_quad_real (int errcond, int errcode, const char *where);
#endif

#if (FLT256_LEN == 15)
static const int QUAD_REAL_ITT_DIV = 3;
static const int QUAD_REAL_K_TANH = 6;
static const int QUAD_REAL_MS_EXP = 39;
static const int QUAD_REAL_MS_HYP = 45;
static const int QUAD_REAL_MS_TRG = 55;
#else
#error invalid real*32 length
#endif

#define QUAD_REAL_MAX_10EX 4931

extern const QUAD_T QUAD_REAL_EE;
extern const QUAD_T QUAD_REAL_FIXCORR;
extern const QUAD_T QUAD_REAL_HALF;
extern const QUAD_T QUAD_REAL_HUNDRED;
extern const QUAD_T QUAD_REAL_LN10;
extern const QUAD_T QUAD_REAL_LN2;
extern const QUAD_T QUAD_REAL_LOG10_E;
extern const QUAD_T QUAD_REAL_LOG2_10;
extern const QUAD_T QUAD_REAL_LOG2_E;
extern const QUAD_T QUAD_REAL_NAN;
extern const QUAD_T QUAD_REAL_ONE;
extern const QUAD_T QUAD_REAL_PI;
extern const QUAD_T QUAD_REAL_PI2;
extern const QUAD_T QUAD_REAL_PI4;
extern const QUAD_T QUAD_REAL_RNDCORR;
extern const QUAD_T QUAD_REAL_SQRT2;
extern const QUAD_T QUAD_REAL_TEN;
extern const QUAD_T QUAD_REAL_TENTH;
extern const QUAD_T QUAD_REAL_THOUSAND;
extern const QUAD_T QUAD_REAL_TWO;
extern const QUAD_T QUAD_REAL_ZERO;

extern const int_2 QUAD_REAL_BIAS;
extern const int_2 QUAD_REAL_DBL_BIAS;
extern const int_2 QUAD_REAL_DBL_LEX;
extern const int_2 QUAD_REAL_DBL_MAX;
extern const int_2 QUAD_REAL_K_LIN;
extern const int_2 QUAD_REAL_MAX_P;
extern const QUAD_T QUAD_REAL_E2MAX;
extern const QUAD_T QUAD_REAL_E2MIN;
extern const QUAD_T QUAD_REAL_EMAX;
extern const QUAD_T QUAD_REAL_EMIN;
extern const QUAD_T QUAD_REAL_MINF;
extern const QUAD_T QUAD_REAL_PINF;
extern const QUAD_T QUAD_REAL_VGV;
extern const QUAD_T QUAD_REAL_VSV;
extern const unt_2 QUAD_REAL_M_EXP;
extern const unt_2 QUAD_REAL_M_SIGN;

extern DOUBLE_T quad_real_to_double_real (QUAD_T);
extern int eq_quad_real (QUAD_T, QUAD_T); 
extern int ge_quad_real (QUAD_T, QUAD_T); 
extern int getexp_quad_real (const QUAD_T *);
extern int getsgn_quad_real (const QUAD_T *);
extern int gt_quad_real (QUAD_T, QUAD_T);  
extern int is0_quad_real (const QUAD_T *);
extern int isMinf_quad_real (const QUAD_T *);
extern int isNaN_quad_real (const QUAD_T *);
extern int isordnumb_quad_real (const QUAD_T *);
extern int isPinf_quad_real (const QUAD_T *);
extern int le_quad_real (QUAD_T, QUAD_T);  
extern int lt_quad_real (QUAD_T, QUAD_T);
extern int neq_quad_real (QUAD_T, QUAD_T);
extern int not0_quad_real (const QUAD_T *);
extern int real_cmp_quad_real (const QUAD_T *, const QUAD_T *);
extern int sgn_quad_real (const QUAD_T *);
extern QUAD_T abs_quad_real (QUAD_T);
extern QUAD_T acosh_quad_real (QUAD_T);
extern QUAD_T acos_quad_real (QUAD_T);
extern QUAD_T acotan_quad_real (QUAD_T);
extern QUAD_T add_quad_real (QUAD_T, QUAD_T, int);
extern QUAD_T aint_quad_real (QUAD_T);
extern QUAD_T asinh_quad_real (QUAD_T);
extern QUAD_T asin_quad_real (QUAD_T);
extern QUAD_T atan2_quad_real (QUAD_T, QUAD_T);
extern QUAD_T atanh_quad_real (QUAD_T);
extern QUAD_T atan_quad_real (QUAD_T);
extern QUAD_T atox (const char *);
extern QUAD_T ceil_quad_real (QUAD_T);
extern QUAD_T* chcof_quad_real (int, QUAD_T (*) (QUAD_T));
extern QUAD_T cosh_quad_real (QUAD_T);
extern QUAD_T cos_quad_real (QUAD_T);
extern QUAD_T cotan_quad_real (QUAD_T);
extern QUAD_T div_quad_real (QUAD_T, QUAD_T);
extern QUAD_T double_real_to_quad_real (DOUBLE_T);
extern QUAD_T evtch_quad_real (QUAD_T, QUAD_T *, int);
extern QUAD_T exp10_quad_real (QUAD_T);
extern QUAD_T exp2_quad_real (QUAD_T);
extern QUAD_T exp_quad_real (QUAD_T);
extern QUAD_T fix_quad_real (QUAD_T);
extern QUAD_T floor_quad_real (QUAD_T);
extern QUAD_T fmod_quad_real (QUAD_T, QUAD_T, QUAD_T *);
extern QUAD_T frac_quad_real (QUAD_T);
extern QUAD_T frexp_quad_real (QUAD_T, int *);
extern QUAD_T int_to_quad_real (int);
extern QUAD_T log10_quad_real (QUAD_T);
extern QUAD_T log2_quad_real (QUAD_T);
extern QUAD_T log_quad_real (QUAD_T);
extern QUAD_T max_quad_real_2 (QUAD_T, QUAD_T);
extern QUAD_T min_quad_real_2 (QUAD_T, QUAD_T);
extern QUAD_T mul_quad_real (QUAD_T, QUAD_T);
extern QUAD_T neg_quad_real (QUAD_T);
extern QUAD_T nint_quad_real (QUAD_T);
extern QUAD_T pow_quad_real (QUAD_T, QUAD_T);
extern QUAD_T pwr_quad_real (QUAD_T, int);
extern QUAD_T real_2_quad_real (QUAD_T, int);
extern QUAD_T real_to_quad_real (REAL_T);
extern QUAD_T round_quad_real (QUAD_T);
extern QUAD_T sfmod_quad_real (QUAD_T, int *);
extern QUAD_T sgn_quad_real_2 (QUAD_T, QUAD_T);
extern QUAD_T sinh_quad_real (QUAD_T);
extern QUAD_T sin_quad_real (QUAD_T);
extern QUAD_T sqrt_quad_real (QUAD_T);
extern QUAD_T string_to_quad_real (const char *, char **);
extern QUAD_T tanh_quad_real (QUAD_T);
extern QUAD_T tan_quad_real (QUAD_T);
extern QUAD_T ten_up_quad_real (int);
extern QUAD_T trunc_quad_real (QUAD_T);
extern REAL_T quad_real_to_real (QUAD_T);
extern void lshift_quad_real (int, unt_2 *, int);
extern void rshift_quad_real (int, unt_2 *, int);

#endif
#endif
