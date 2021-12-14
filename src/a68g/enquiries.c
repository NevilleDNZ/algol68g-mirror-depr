//! @file enquiries.c
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

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-physics.h"
#include "a68g-numbers.h"
#include "a68g-mp.h"

// Environment enquiries.

A68_ENV_INT (genie_int_lengths, 3);
A68_ENV_INT (genie_int_shorths, 1);
A68_ENV_INT (genie_real_lengths, 3);
A68_ENV_INT (genie_real_shorths, 1);
A68_ENV_INT (genie_complex_lengths, 3);
A68_ENV_INT (genie_complex_shorths, 1);
#if (A68_LEVEL >= 3)
A68_ENV_INT (genie_bits_lengths, 2);
#else
A68_ENV_INT (genie_bits_lengths, 3);
#endif
A68_ENV_INT (genie_bits_shorths, 1);
A68_ENV_INT (genie_bytes_lengths, 2);
A68_ENV_INT (genie_bytes_shorths, 1);
A68_ENV_INT (genie_int_width, INT_WIDTH);
A68_ENV_INT (genie_long_int_width, LONG_INT_WIDTH);
A68_ENV_INT (genie_long_mp_int_width, LONG_LONG_INT_WIDTH);
A68_ENV_INT (genie_real_width, REAL_WIDTH);
A68_ENV_INT (genie_long_real_width, LONG_REAL_WIDTH);
A68_ENV_INT (genie_long_mp_real_width, LONG_LONG_REAL_WIDTH);
A68_ENV_INT (genie_exp_width, EXP_WIDTH);
A68_ENV_INT (genie_long_exp_width, LONG_EXP_WIDTH);
A68_ENV_INT (genie_long_mp_exp_width, LONG_LONG_EXP_WIDTH);
A68_ENV_INT (genie_bits_width, BITS_WIDTH);
#if (A68_LEVEL >= 3)
A68_ENV_INT (genie_long_bits_width, LONG_BITS_WIDTH);
#else
A68_ENV_INT (genie_long_bits_width, get_mp_bits_width (M_LONG_BITS));
A68_ENV_INT (genie_long_mp_bits_width, get_mp_bits_width (M_LONG_LONG_BITS));
#endif
A68_ENV_INT (genie_bytes_width, BYTES_WIDTH);
A68_ENV_INT (genie_long_bytes_width, LONG_BYTES_WIDTH);
A68_ENV_INT (genie_max_abs_char, UCHAR_MAX);
A68_ENV_INT (genie_max_int, A68_MAX_INT);
A68_ENV_INT (genie_mp_radix, MP_RADIX);
A68_ENV_INT (genie_stack_pointer, A68_SP);
A68_ENV_INT (genie_system_stack_size, A68 (stack_size));
A68_ENV_REAL (genie_cputime, seconds () - A68 (cputime_0));
A68_ENV_REAL (genie_max_real, REAL_MAX);
A68_ENV_REAL (genie_min_real, REAL_MIN);
A68_ENV_REAL (genie_pi, CONST_PI);
A68_ENV_REAL (genie_small_real, REAL_EPSILON);
