//! @file equivalence.c
//! @author J. Marcel van der Veer
//
//! @section Copyright
//
// This file is part of Algol68G - an Algol 68 compiler-interpreter.
// Copyright 2001-2022 J. Marcel van der Veer <algol68g@xs4all.nl>.
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
#include "a68g-postulates.h"
#include "a68g-parser.h"

// Routines for establishing equivalence of modes.
//
// After I made this mode equivalencer (in 1993), I found:
// Algol Bulletin 30.3.3 C.H.A. Koster: On infinite modes, 86-89 [1969],
// which essentially concurs with this test on mode equivalence I wrote.
// It is elemntary logic: proving equivalence, assuming equivalence.

//! @brief Whether packs are equivalent, same sequence of equivalence modes.

static BOOL_T is_packs_equivalent (PACK_T * s, PACK_T * t)
{
  for (; s != NO_PACK && t != NO_PACK; FORWARD (s), FORWARD (t)) {
    if (!is_modes_equivalent (MOID (s), MOID (t))) {
      return A68_FALSE;
    }
    if (TEXT (s) != TEXT (t)) {
      return A68_FALSE;
    }
  }
  return (BOOL_T) (s == NO_PACK && t == NO_PACK);
}

//! @brief Whether packs are equivalent, must be subsets.

static BOOL_T is_united_packs_equivalent (PACK_T * s, PACK_T * t)
{
  PACK_T *p;
// whether s is a subset of t ....
  for (p = s; p != NO_PACK; FORWARD (p)) {
    BOOL_T f;
    PACK_T *q;
    for (f = A68_FALSE, q = t; q != NO_PACK && !f; FORWARD (q)) {
      f = is_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return A68_FALSE;
    }
  }
// ... and whether t is a subset of s.
  for (p = t; p != NO_PACK; FORWARD (p)) {
    BOOL_T f;
    PACK_T *q;
    for (f = A68_FALSE, q = s; q != NO_PACK && !f; FORWARD (q)) {
      f = is_modes_equivalent (MOID (p), MOID (q));
    }
    if (!f) {
      return A68_FALSE;
    }
  }
  return A68_TRUE;
}

//! @brief Whether moids a and b are structurally equivalent.

BOOL_T is_modes_equivalent (MOID_T * a, MOID_T * b)
{
  if (a == NO_MOID || b == NO_MOID) {
// Modes can be NO_MOID in partial argument lists.
    return A68_FALSE;
  } else if (a == b) {
    return A68_TRUE;
  } else if (a == M_ERROR || b == M_ERROR) {
    return A68_FALSE;
  } else if (ATTRIBUTE (a) != ATTRIBUTE (b)) {
    return A68_FALSE;
  } else if (DIM (a) != DIM (b)) {
    return A68_FALSE;
  } else if (IS (a, STANDARD)) {
    return (BOOL_T) (a == b);
  } else if (EQUIVALENT (a) == b || EQUIVALENT (b) == a) {
    return A68_TRUE;
  } else if (is_postulated_pair (A68 (top_postulate), a, b) || is_postulated_pair (A68 (top_postulate), b, a)) {
    return A68_TRUE;
  } else if (IS (a, INDICANT)) {
    if (NODE (a) == NO_NODE || NODE (b) == NO_NODE) {
      return A68_FALSE;
    } else {
      return NODE (a) == NODE (b);
    }
  }
  switch (ATTRIBUTE (a)) {
  case REF_SYMBOL:
  case ROW_SYMBOL:
  case FLEX_SYMBOL:{
      return is_modes_equivalent (SUB (a), SUB (b));
    }
  }
  if (IS (a, PROC_SYMBOL) && PACK (a) == NO_PACK && PACK (b) == NO_PACK) {
    return is_modes_equivalent (SUB (a), SUB (b));
  } else if (IS (a, STRUCT_SYMBOL)) {
    POSTULATE_T *save;
    BOOL_T z;
    save = A68 (top_postulate);
    make_postulate (&A68 (top_postulate), a, b);
    z = is_packs_equivalent (PACK (a), PACK (b));
    free_postulate_list (A68 (top_postulate), save);
    A68 (top_postulate) = save;
    return z;
  } else if (IS (a, UNION_SYMBOL)) {
    return is_united_packs_equivalent (PACK (a), PACK (b));
  } else if (IS (a, PROC_SYMBOL) && PACK (a) != NO_PACK && PACK (b) != NO_PACK) {
    POSTULATE_T *save;
    BOOL_T z;
    save = A68 (top_postulate);
    make_postulate (&A68 (top_postulate), a, b);
    z = is_modes_equivalent (SUB (a), SUB (b));
    if (z) {
      z = is_packs_equivalent (PACK (a), PACK (b));
    }
    free_postulate_list (A68 (top_postulate), save);
    A68 (top_postulate) = save;
    return z;
  } else if (IS (a, SERIES_MODE) || IS (a, STOWED_MODE)) {
    return is_packs_equivalent (PACK (a), PACK (b));
  }
  return A68_FALSE;
}

//! @brief Whether modes 1 and 2 are structurally equivalent.

BOOL_T prove_moid_equivalence (MOID_T * p, MOID_T * q)
{
// Prove two modes to be equivalent under assumption that they indeed are.
  POSTULATE_T *save = A68 (top_postulate);
  BOOL_T z = is_modes_equivalent (p, q);
  free_postulate_list (A68 (top_postulate), save);
  A68 (top_postulate) = save;
  return z;
}
