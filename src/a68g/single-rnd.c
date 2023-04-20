//! @file single-rnd.c
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
//! REAL pseudo-random number generator.

#include "a68g.h"
#include "a68g-genie.h"
#include "a68g-prelude.h"
#include "a68g-double.h"
#include "a68g-numbers.h"

// Next part is a "stand-alone" version of GNU Scientific Library (GSL)
// random number generator "taus113", based on GSL file "rng/taus113.c".

// rng/taus113.c
// Copyright (C) 2002 Atakan Gurkan
// Based on the file taus.c which has the notice
// Copyright (C) 1996, 1997, 1998, 1999, 2000, 2007 James Theiler, Brian Gough
// 
// This program is free software; you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation; either version 3 of the License, or (at
// your option) any later version.
// 
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
// General Public License for more details.
// 
// You should have received a copy of the GNU General Public License
// along with this program; if not, write to the Free Software
// Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
//
// This is a maximally equidistributed combined, collision free 
// Tausworthe generator, with a period ~2^{113}. The sequence is,
//
//   x_n = (z1_n ^ z2_n ^ z3_n ^ z4_n)  
//
//   b = (((z1_n <<  6) ^ z1_n) >> 13)
//   z1_{n+1} = (((z1_n & 4294967294) << 18) ^ b)
//   b = (((z2_n <<  2) ^ z2_n) >> 27)
//   z2_{n+1} = (((z2_n & 4294967288) <<  2) ^ b)
//   b = (((z3_n << 13) ^ z3_n) >> 21)
//   z3_{n+1} = (((z3_n & 4294967280) <<  7) ^ b)
//   b = (((z4_n <<  3)  ^ z4_n) >> 12)
//   z4_{n+1} = (((z4_n & 4294967168) << 13) ^ b)
//
// computed modulo 2^32. In the formulas above '^' means exclusive-or 
// (C-notation), not exponentiation. 
// The algorithm is for 32-bit integers, hence a bitmask is used to clear 
// all but least significant 32 bits, after left shifts, to make the code 
// work on architectures where integers are 64-bit.
//
// The generator is initialized with 
// z{i+1} = (69069 * zi) MOD 2^32 where z0 is the seed provided
// During initialization a check is done to make sure that the initial seeds 
// have a required number of their most significant bits set.
// After this, the state is passed through the RNG 10 times to ensure the
// state satisfies a recurrence relation.
//
// References:
//   P. L'Ecuyer, "Tables of Maximally-Equidistributed Combined LFSR Generators",
//   Mathematics of Computation, 68, 225 (1999), 261--269.
//     http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme2.ps
//   P. L'Ecuyer, "Maximally Equidistributed Combined Tausworthe Generators", 
//   Mathematics of Computation, 65, 213 (1996), 203--213.
//     http://www.iro.umontreal.ca/~lecuyer/myftp/papers/tausme.ps
//   the online version of the latter contains corrections to the print version.

#define LCG(n) ((69069UL * n) & 0xffffffffUL)
#define MASK 0xffffffffUL

unt taus113_get (void *vstate);
double taus113_get_double (void *vstate);
void taus113_set (void *state, unt long int s);

typedef struct
{
  unt long int z1, z2, z3, z4;
}
taus113_state_t;

static taus113_state_t rng_state;

unt taus113_get (void *vstate)
{
  taus113_state_t *state = (taus113_state_t *) vstate;
  unt long b1, b2, b3, b4;
  b1 = ((((state->z1 << 6UL) & MASK) ^ state->z1) >> 13UL);
  state->z1 = ((((state->z1 & 4294967294UL) << 18UL) & MASK) ^ b1);
  b2 = ((((state->z2 << 2UL) & MASK) ^ state->z2) >> 27UL);
  state->z2 = ((((state->z2 & 4294967288UL) << 2UL) & MASK) ^ b2);
  b3 = ((((state->z3 << 13UL) & MASK) ^ state->z3) >> 21UL);
  state->z3 = ((((state->z3 & 4294967280UL) << 7UL) & MASK) ^ b3);
  b4 = ((((state->z4 << 3UL) & MASK) ^ state->z4) >> 12UL);
  state->z4 = ((((state->z4 & 4294967168UL) << 13UL) & MASK) ^ b4);
  return (state->z1 ^ state->z2 ^ state->z3 ^ state->z4);
}

double taus113_get_double (void *vstate)
{
  return taus113_get (vstate) / 4294967296.0;
}

void taus113_set (void *vstate, unt long int s)
{
  taus113_state_t *state = (taus113_state_t *) vstate;
  if (!s) {
    s = 1UL; // default seed is 1
  }
  state->z1 = LCG (s);
  if (state->z1 < 2UL) {
    state->z1 += 2UL;
  }
  state->z2 = LCG (state->z1);
  if (state->z2 < 8UL) {
    state->z2 += 8UL;
  }
  state->z3 = LCG (state->z2);
  if (state->z3 < 16UL) {
    state->z3 += 16UL;
  }
  state->z4 = LCG (state->z3);
  if (state->z4 < 128UL) {
    state->z4 += 128UL;
  }
// Calling RNG ten times to satify recurrence condition
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  taus113_get (state);
  return;
}

// Initialise rng.

void init_rng (unt u)
{
  taus113_set (&rng_state, u);
}

// A68G rng in R mathlib style.

REAL_T a68_unif_rand (void)
{
// In [0, 1>
  return taus113_get_double (&rng_state);
}

static char *state_file = ".Random.seed";

void GetRNGstate (void)
{
  INT_T fd = open (state_file, A68_READ_ACCESS);
  if (fd != -1) {
    ASSERT (read (fd, &rng_state, sizeof (taus113_state_t)) != -1);
    close (fd);
  }
}

void PutRNGstate (void)
{
  INT_T fd = open (state_file, A68_WRITE_ACCESS, A68_PROTECTION);
  if (fd != -1) {
    ASSERT (write (fd, &rng_state, sizeof (taus113_state_t)) != -1);
    close (fd);
  }
}
