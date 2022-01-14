//! @file postulates.c
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

// Postulates are for proving A assuming A is true.

#include "a68g.h"
#include "a68g-prelude.h"
#include "a68g-mp.h"
#include "a68g-genie.h"
#include "a68g-postulates.h"

//! @brief Initialise use of postulate-lists.

void init_postulates (void)
{
  A68 (top_postulate) = NO_POSTULATE;
  A68 (top_postulate_list) = NO_POSTULATE;
}

//! @brief Make old postulates available for new use.

void free_postulate_list (POSTULATE_T * start, POSTULATE_T * stop)
{
  POSTULATE_T *last;
  if (start == stop) {
    return;
  }
  for (last = start; NEXT (last) != stop; FORWARD (last)) {
    ;
  }
  NEXT (last) = A68 (top_postulate_list);
  A68 (top_postulate_list) = start;
}

//! @brief Add postulates to postulate-list.

void make_postulate (POSTULATE_T ** p, MOID_T * a, MOID_T * b)
{
  POSTULATE_T *new_one;
  if (A68 (top_postulate_list) != NO_POSTULATE) {
    new_one = A68 (top_postulate_list);
    FORWARD (A68 (top_postulate_list));
  } else {
    new_one = (POSTULATE_T *) get_temp_heap_space ((size_t) SIZE_ALIGNED (POSTULATE_T));
    A68 (new_postulates)++;
  }
  A (new_one) = a;
  B (new_one) = b;
  NEXT (new_one) = *p;
  *p = new_one;
}

//! @brief Where postulates are in the list.

POSTULATE_T *is_postulated_pair (POSTULATE_T * p, MOID_T * a, MOID_T * b)
{
  for (; p != NO_POSTULATE; FORWARD (p)) {
    if (A (p) == a && B (p) == b) {
      return p;
    }
  }
  return NO_POSTULATE;
}

//! @brief Where postulate is in the list.

POSTULATE_T *is_postulated (POSTULATE_T * p, MOID_T * a)
{
  for (; p != NO_POSTULATE; FORWARD (p)) {
    if (A (p) == a) {
      return p;
    }
  }
  return NO_POSTULATE;
}
