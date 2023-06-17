//! @file a68g-conversion.c
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
//! Conversion tables for IEEE platforms.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"
#include "a68g-parser.h"
#include "a68g-options.h"
#include "a68g-optimiser.h"
#include "a68g-listing.h"

// A list of 10 ^ 2 ^ n for conversion purposes on IEEE 754 platforms.

#if (A68_LEVEL >= 3)

//! @brief 10 ** expo

static DOUBLE_T pow_10_double[] = {
  10.0q, 100.0q, 1.0e4q, 1.0e8q, 1.0e16q, 1.0e32q, 1.0e64q, 1.0e128q, 1.0e256q, 1.0e512q, 1.0e1024q, 1.0e2048q, 1.0e4096q
};

DOUBLE_T ten_up_double (int expo)
{
// This way appears sufficiently accurate.
  DOUBLE_T dbl_expo = 1.0q, *dep;
  BOOL_T neg_expo;
  if (expo == 0) {
    return 1.0q;
  }
  neg_expo = (BOOL_T) (expo < 0);
  if (neg_expo) {
    expo = -expo;
  }
  if (expo > MAX_DOUBLE_EXPO) {
    expo = 0;
    errno = EDOM;
  }
  ABEND (expo > MAX_DOUBLE_EXPO, ERROR_INVALID_VALUE, __func__);
  for (dep = pow_10_double; expo != 0; expo >>= 1, dep++) {
    if (expo & 0x1) {
      dbl_expo *= *dep;
    }
  }
  return neg_expo ? 1.0q / dbl_expo : dbl_expo;
}

#endif

static REAL_T pow_10[] = {
  10.0, 100.0, 1.0e4, 1.0e8, 1.0e16, 1.0e32, 1.0e64, 1.0e128, 1.0e256
};

//! @brief 10 ** expo

REAL_T ten_up (int expo)
{
// This way appears sufficiently accurate.
  REAL_T dbl_expo = 1.0, *dep;
  BOOL_T neg_expo = (BOOL_T) (expo < 0);
  if (neg_expo) {
    expo = -expo;
  }
  ABEND (expo > MAX_REAL_EXPO, ERROR_INVALID_VALUE, __func__);
  for (dep = pow_10; expo != 0; expo >>= 1, dep++) {
    if (expo & 0x1) {
      dbl_expo *= *dep;
    }
  }
  return neg_expo ? 1 / dbl_expo : dbl_expo;
}
