//! @file a68g-stddef.h
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

#if !defined (__A68G_STDDEF_H__)
#define __A68G_STDDEF_H__ 

#define DOUBLE_ACCURACY (REAL_DIG - 1)

// Multi-precision parameters
#define LONG_LONG_EXP_WIDTH (EXP_WIDTH)
#define LONG_LONG_INT_WIDTH (1 + LONG_LONG_WIDTH)
#define LONG_LONG_REAL_WIDTH ((long_mp_digits() - 1) * LOG_MP_RADIX)
#define LONG_LONG_WIDTH (long_mp_digits() * LOG_MP_RADIX)

// Other WIDTHs
#define BYTES_WIDTH 32
#define LONG_BYTES_WIDTH 256
#define MAX_REAL_EXPO 511

#define LOG_DBL_EPSILON   (-3.6043653389117154e+01)
#define LOG_DBL_MIN       (-7.0839641853226408e+02)
#define LOG_DBL_MAX       (7.0978271289338397e+02)

#define REAL_DIG DBL_DIG
#define REAL_EPSILON DBL_EPSILON
#define REAL_MANT_DIG DBL_MANT_DIG
#define REAL_MAX DBL_MAX
#define REAL_MAX_10_EXP DBL_MAX_10_EXP
#define REAL_MIN DBL_MIN
#define REAL_MIN_10_EXP DBL_MIN_10_EXP

#if (A68_LEVEL >= 3)

#define REAL_WIDTH (REAL_DIG)
#define MAX_REAL_double_real_EXPO 4932
#define A68_MAX_INT (LLONG_MAX)
#define A68_MAX_BITS (ULLONG_MAX)
#define LONG_WIDTH (2 * INT_WIDTH + 1)
#define LONG_REAL_WIDTH (FLT128_DIG - 1)
#define EXP_WIDTH (3)
#define LONG_EXP_WIDTH (4)
#define LONG_BITS_WIDTH (2 * BITS_WIDTH)
#define D_SIGN 0x8000000000000000LL
#define NaN_MP ((MP_T *) NULL)
#define MP_RADIX 1000000000
#define LOG_MP_RADIX 9
#define MP_RADIX_Q MP_RADIX##q
#define DEFAULT_DOUBLE_DIGITS 4
#define LONG_MP_DIGITS DEFAULT_DOUBLE_DIGITS
#define LONG_LONG_MP_DIGITS width_to_mp_digits (4 * REAL_DIG + REAL_DIG / 2)
#define MAX_MP_EXPONENT 111111             // Arbitrary. Largest range is A68_MAX_INT / Log A68_MAX_INT / LOG_MP_RADIX
#define MAX_REPR_INT 9223372036854775808.0 // 2^63, max int in an extended double (no implicit bit, so 62-bit mantissa)
#define MAX_DOUBLE_EXPO 4932

#else // if (A68_LEVEL <= 2)

#define REAL_WIDTH (REAL_DIG)
#define A68_MAX_INT (INT_MAX)
#define A68_MAX_BITS (UINT_MAX)
#define LONG_WIDTH (LONG_MP_DIGITS * LOG_MP_RADIX)
#define LONG_REAL_WIDTH ((LONG_MP_DIGITS - 1) * LOG_MP_RADIX)
#define EXP_WIDTH ((int) (1 + log10 ((REAL_T) REAL_MAX_10_EXP)))
#define LONG_EXP_WIDTH (EXP_WIDTH)
#define D_SIGN 0x80000000L
#define MP_BITS_BITS 23
#define MP_BITS_RADIX 8388608   // Max power of two smaller than MP_RADIX
#define NaN_MP ((MP_T *) NULL)
#define MP_RADIX 10000000
#define MP_RADIX_Q MP_RADIX##q
#define LOG_MP_RADIX 7
#define DEFAULT_DOUBLE_DIGITS 6
#define LONG_MP_DIGITS DEFAULT_DOUBLE_DIGITS
#define LONG_LONG_MP_DIGITS width_to_mp_digits (4 * REAL_DIG + REAL_DIG / 2)
#define MAX_MP_EXPONENT 142857          // Arbitrary. Largest range is A68_MAX_INT / Log A68_MAX_INT / LOG_MP_RADIX
#define MAX_REPR_INT 9007199254740992.0 // 2^53, max int in a double

#endif

#endif
