//! @file prelude-bits.c
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
//! Multiple precision BITS.

#include "a68g.h"

#if (A68_LEVEL <= 2)

#include "a68g-optimiser.h"
#include "a68g-prelude.h"
#include "a68g-transput.h"
#include "a68g-mp.h"
#include "a68g-parser.h"
#include "a68g-physics.h"
#include "a68g-double.h"

#define A68_STD A68_TRUE
#define A68_EXT A68_FALSE

void stand_longlong_bits (void)
{
  MOID_T *m;
// LONG LONG BITS in software, vintage
  a68_mode (2, "BITS", &M_LONG_LONG_BITS);
// REF LONG LONG BITS
  MODE (REF_LONG_LONG_BITS) = add_mode (&TOP_MOID (&A68_JOB), REF_SYMBOL, 0, NO_NODE, M_LONG_LONG_BITS, NO_PACK);
// [] LONG LONG BITS
  M_ROW_LONG_LONG_BITS = add_mode (&TOP_MOID (&A68_JOB), ROW_SYMBOL, 1, NO_NODE, M_LONG_LONG_BITS, NO_PACK);
  HAS_ROWS (M_ROW_LONG_LONG_BITS) = A68_TRUE;
  SLICE (M_ROW_LONG_LONG_BITS) = M_LONG_LONG_BITS;
//
  a68_idf (A68_STD, "longlongbitswidth", M_INT, genie_long_mp_bits_width);
  a68_idf (A68_STD, "longlongmaxbits", M_LONG_LONG_BITS, genie_long_mp_max_bits);
//
  m = a68_proc (M_LONG_LONG_BITS, M_ROW_BOOL, NO_MOID);
  a68_idf (A68_STD, "longlongbitspack", m, genie_long_bits_pack);
  A68C_DEFIO (longlongbits, long_mp_bits, LONG_LONG_BITS);
//
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_idle);
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "LENG", m, genie_lengthen_mp_to_long_mp);
  m = a68_proc (M_LONG_LONG_INT, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ABS", m, genie_idle);
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_LONG_INT, NO_MOID);
  a68_op (A68_STD, "BIN", m, genie_bin_mp);
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "NOT", m, genie_not_mp);
  a68_op (A68_STD, "~", m, genie_not_mp);
  m = a68_proc (M_LONG_BITS, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SHORTEN", m, genie_shorten_long_mp_to_mp);
  m = a68_proc (M_BOOL, M_LONG_LONG_BITS, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "=", m, genie_eq_mp);
  a68_op (A68_STD, "EQ", m, genie_eq_mp);
  a68_op (A68_STD, "/=", m, genie_ne_mp);
  a68_op (A68_STD, "~=", m, genie_ne_mp);
  a68_op (A68_STD, "^=", m, genie_ne_mp);
  a68_op (A68_STD, "NE", m, genie_ne_mp);
  a68_op (A68_STD, "<=", m, genie_le_mp);
  a68_op (A68_STD, "LE", m, genie_le_mp);
  a68_op (A68_STD, ">=", m, genie_ge_mp);
  a68_op (A68_STD, "GE", m, genie_ge_mp);
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_LONG_BITS, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "AND", m, genie_and_mp);
  a68_op (A68_STD, "&", m, genie_and_mp);
  a68_op (A68_STD, "OR", m, genie_or_mp);
  a68_op (A68_EXT, "XOR", m, genie_xor_mp);
  m = a68_proc (M_LONG_LONG_BITS, M_LONG_LONG_BITS, M_INT, NO_MOID);
  a68_op (A68_STD, "SHL", m, genie_shl_mp);
  a68_op (A68_STD, "UP", m, genie_shl_mp);
  a68_op (A68_STD, "SHR", m, genie_shr_mp);
  a68_op (A68_STD, "DOWN", m, genie_shr_mp);
  m = a68_proc (M_BOOL, M_INT, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "ELEM", m, genie_elem_long_mp_bits);
  m = a68_proc (M_LONG_LONG_BITS, M_INT, M_LONG_LONG_BITS, NO_MOID);
  a68_op (A68_STD, "SET", m, genie_set_long_mp_bits);
  a68_op (A68_STD, "CLEAR", m, genie_clear_long_mp_bits);
}

#endif
