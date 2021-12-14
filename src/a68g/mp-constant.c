//! @file mp-constant.c
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
#include "a68g-double.h"
#include "a68g-mp.h"

//! @brief Return "pi" with "digs" precision, using Borwein & Borwein AGM.

MP_T *mp_pi (NODE_T * p, MP_T * api, int mod, int digs)
{
  int gdigs = FUN_DIGITS (digs);
  if (gdigs > A68_MP (mp_pi_size)) {
// No luck with the kept value, hence we generate a longer "pi".
// Calculate "pi" using the Borwein & Borwein AGM algorithm.
// This AGM doubles the numbers of digs every iteration.
    a68_free (A68_MP (mp_pi));
    a68_free (A68_MP (mp_half_pi));
    a68_free (A68_MP (mp_two_pi));
    a68_free (A68_MP (mp_sqrt_two_pi));
    a68_free (A68_MP (mp_sqrt_pi));
    a68_free (A68_MP (mp_ln_pi));
    a68_free (A68_MP (mp_180_over_pi));
    a68_free (A68_MP (mp_pi_over_180));
//
    ADDR_T pop_sp = A68_SP;
    MP_T *pi_g = nil_mp (p, gdigs);
    MP_T *two = lit_mp (p, 2, 0, gdigs);
    MP_T *x_g = lit_mp (p, 2, 0, gdigs);
    MP_T *y_g = nil_mp (p, gdigs);
    MP_T *u_g = nil_mp (p, gdigs);
    MP_T *v_g = nil_mp (p, gdigs);
    (void) sqrt_mp (p, x_g, x_g, gdigs);
    (void) add_mp (p, pi_g, x_g, two, gdigs);
    (void) sqrt_mp (p, y_g, x_g, gdigs);
    BOOL_T iterate = A68_TRUE;
    while (iterate) {
// New x.
      (void) sqrt_mp (p, u_g, x_g, gdigs);
      (void) rec_mp (p, v_g, u_g, gdigs);
      (void) add_mp (p, u_g, u_g, v_g, gdigs);
      (void) half_mp (p, x_g, u_g, gdigs);
// New pi.
      (void) plus_one_mp (p, u_g, x_g, gdigs);
      (void) plus_one_mp (p, v_g, y_g, gdigs);
      (void) div_mp (p, u_g, u_g, v_g, gdigs);
      (void) mul_mp (p, v_g, pi_g, u_g, gdigs);
// Done yet?.
      if (same_mp (p, v_g, pi_g, gdigs)) {
        iterate = A68_FALSE;
      } else {
        (void) move_mp (pi_g, v_g, gdigs);
// New y.
        (void) sqrt_mp (p, u_g, x_g, gdigs);
        (void) rec_mp (p, v_g, u_g, gdigs);
        (void) mul_mp (p, u_g, y_g, u_g, gdigs);
        (void) add_mp (p, u_g, u_g, v_g, gdigs);
        (void) plus_one_mp (p, v_g, y_g, gdigs);
        (void) div_mp (p, y_g, u_g, v_g, gdigs);
      }
    }
// Keep the result for future restore.
    (void) shorten_mp (p, api, digs, pi_g, gdigs);
    A68_MP (mp_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) move_mp (A68_MP (mp_pi), api, digs);
    A68_MP (mp_half_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) half_mp (p, A68_MP (mp_half_pi), api, digs);
    A68_MP (mp_sqrt_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) sqrt_mp (p, A68_MP (mp_sqrt_pi), api, digs);
    A68_MP (mp_ln_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) ln_mp (p, A68_MP (mp_ln_pi), api, digs);
    A68_MP (mp_two_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) mul_mp_digit (p, A68_MP (mp_two_pi), api, (MP_T) 2, digs);
    A68_MP (mp_sqrt_two_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) sqrt_mp (p, A68_MP (mp_sqrt_two_pi), A68_MP (mp_two_pi), digs);
    A68_MP (mp_pi_over_180) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) div_mp_digit (p, A68_MP (mp_pi_over_180), api, 180, digs);
    A68_MP (mp_180_over_pi) = (MP_T *) get_heap_space ((unsigned) SIZE_MP (digs));
    (void) rec_mp (p, A68_MP (mp_180_over_pi), A68_MP (mp_pi_over_180), digs);
    A68_MP (mp_pi_size) = gdigs;
    A68_SP = pop_sp;
  }
  switch (mod) {
  case MP_PI:          return move_mp (api, A68_MP (mp_pi), digs);
  case MP_HALF_PI:     return move_mp (api, A68_MP (mp_half_pi), digs);
  case MP_TWO_PI:      return move_mp (api, A68_MP (mp_two_pi), digs);
  case MP_SQRT_TWO_PI: return move_mp (api, A68_MP (mp_sqrt_two_pi), digs);
  case MP_SQRT_PI:     return move_mp (api, A68_MP (mp_sqrt_pi), digs);
  case MP_LN_PI:       return move_mp (api, A68_MP (mp_ln_pi), digs);
  case MP_180_OVER_PI: return move_mp (api, A68_MP (mp_180_over_pi), digs);
  case MP_PI_OVER_180: return move_mp (api, A68_MP (mp_pi_over_180), digs);
  default:             return NaN_MP; // Should not be here.
  }
}

