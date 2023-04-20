//! @file a68g-level-3.h
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
//! Platform dependent definitions.

#if !defined (__A68G_LEVEL_3_H__)
#define __A68G_LEVEL_3_H__

typedef long long int INT_T;
typedef long long unt UNSIGNED_T;
typedef UNSIGNED_T ADDR_T;
typedef __float128 DOUBLE_T;
typedef struct A68_DOUBLE A68_DOUBLE, A68_LONG_INT, A68_LONG_REAL, A68_LONG_BITS;
typedef DOUBLE_T A68_ALIGN_T;

typedef union DOUBLE_NUM_T DOUBLE_NUM_T;

union DOUBLE_NUM_T
{
  UNSIGNED_T u[2];
  DOUBLE_T f;
};

struct A68_DOUBLE
{
  STATUS_MASK_T status;
  DOUBLE_NUM_T value;
} ALIGNED;

typedef A68_LONG_REAL A68_LONG_COMPLEX[2];
#define DOUBLE_COMPLEX_T __complex128

#define a68_strtoi strtoll
#define a68_strtou strtoull

#define A68_LD "%lld"
#define A68_LU "%llu"
#define A68_LX "%llx"

#define A68_FRAME_ALIGN(s) (A68_ALIGN(s))
#define SIGNQ(n) ((n) == 0.0q ? 0 : ((n) > 0 ? 1 : -1))

extern void standardise_double (DOUBLE_T *, int, int, int *);
extern DOUBLE_T ten_up_double (int);
extern BOOL_T convert_radix_double (NODE_T *, DOUBLE_NUM_T, int, int);

typedef __float80 MP_REAL_T;
typedef INT_T MP_INT_T;
typedef UNSIGNED_T MP_BITS_T;
typedef INT_T MP_T;

#define FLOOR_MP floorl
#define DOUBLE_MIN_10_EXP LDBL_MIN_10_EXP

#endif
