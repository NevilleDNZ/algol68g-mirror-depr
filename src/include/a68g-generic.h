//! @file a68g-generic.h
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

#if !defined (__A68G_LEGACY_H__)
#define __A68G_LEGACY_H__

typedef int INT_T;
typedef unt UNSIGNED_T;
typedef UNSIGNED_T ADDR_T;
typedef REAL_T A68_LONG_COMPLEX[2];
typedef REAL_T DOUBLE_T;

typedef int *A68_ALIGN_T;

#define A68_LD "%d"
#define A68_LU "%u"
#define A68_LX "%x"

#define a68_strtoi strtol
#define a68_strtou strtoul

#define A68_FRAME_ALIGN(s) ((int) ((s) % 8) == 0 ? (s) : ((s) - (s) % 8 + 8))

typedef REAL_T MP_REAL_T;
typedef int MP_INT_T;
typedef unt MP_BITS_T;
typedef MP_REAL_T MP_T;

#define FLOOR_MP floor

extern void stand_longlong_bits (void);

#endif
