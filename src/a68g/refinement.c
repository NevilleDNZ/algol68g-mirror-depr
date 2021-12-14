//! @file refinement.c
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
#include "a68g-parser.h"

// This code implements a small refinement preprocessor for A68G.
//
// At the University of Nijmegen a preprocessor much like this one was used
// as a front-end to FLACC in elementary computer science courses.
// See: 
//
// C.H.A. Koster et al., 
// Systematisch programmeren in Algol 68, Deel I en II.

//! @brief Whether refinement terminator.

static BOOL_T is_refinement_terminator (NODE_T * p)
{
  if (IS (p, POINT_SYMBOL)) {
    if (IN_PRELUDE (NEXT (p))) {
      return A68_TRUE;
    } else {
      return whether (p, POINT_SYMBOL, IDENTIFIER, COLON_SYMBOL, STOP);
    }
  } else {
    return A68_FALSE;
  }
}

//! @brief Get refinement definitions in the internal source.

void get_refinements (void)
{
  NODE_T *p = TOP_NODE (&(A68 (job)));
  TOP_REFINEMENT (&(A68 (job))) = NO_REFINEMENT;
// First look where the prelude ends.
  while (p != NO_NODE && IN_PRELUDE (p)) {
    FORWARD (p);
  }
// Determine whether the program contains refinements at all.
  while (p != NO_NODE && !IN_PRELUDE (p) && !is_refinement_terminator (p)) {
    FORWARD (p);
  }
  if (p == NO_NODE || IN_PRELUDE (p)) {
    return;
  }
// Apparently this is code with refinements.
  FORWARD (p);
  if (p == NO_NODE || IN_PRELUDE (p)) {
// Ok, we accept a program with no refinements as well.
    return;
  }
  while (p != NO_NODE && !IN_PRELUDE (p) && whether (p, IDENTIFIER, COLON_SYMBOL, STOP)) {
    REFINEMENT_T *new_one = (REFINEMENT_T *) get_fixed_heap_space ((size_t) SIZE_ALIGNED (REFINEMENT_T)), *x;
    BOOL_T exists;
    NEXT (new_one) = NO_REFINEMENT;
    NAME (new_one) = NSYMBOL (p);
    APPLICATIONS (new_one) = 0;
    LINE_DEFINED (new_one) = LINE (INFO (p));
    LINE_APPLIED (new_one) = NO_LINE;
    NODE_DEFINED (new_one) = p;
    BEGIN (new_one) = END (new_one) = NO_NODE;
    p = NEXT_NEXT (p);
    if (p == NO_NODE) {
      diagnostic (A68_SYNTAX_ERROR, NO_NODE, ERROR_REFINEMENT_EMPTY);
      return;
    } else {
      BEGIN (new_one) = p;
    }
    while (p != NO_NODE && ATTRIBUTE (p) != POINT_SYMBOL) {
      END (new_one) = p;
      FORWARD (p);
    }
    if (p == NO_NODE) {
      diagnostic (A68_SYNTAX_ERROR, NO_NODE, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL);
      return;
    } else {
      FORWARD (p);
    }
// Do we already have one by this name.
    x = TOP_REFINEMENT (&(A68 (job)));
    exists = A68_FALSE;
    while (x != NO_REFINEMENT && !exists) {
      if (NAME (x) == NAME (new_one)) {
        diagnostic (A68_SYNTAX_ERROR, NODE_DEFINED (new_one), ERROR_REFINEMENT_DEFINED);
        exists = A68_TRUE;
      }
      FORWARD (x);
    }
// Straight insertion in chain.
    if (!exists) {
      NEXT (new_one) = TOP_REFINEMENT (&(A68 (job)));
      TOP_REFINEMENT (&(A68 (job))) = new_one;
    }
  }
  if (p != NO_NODE && !IN_PRELUDE (p)) {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_REFINEMENT_INVALID);
  }
}

//! @brief Put refinement applications in the internal source.

void put_refinements (void)
{
  REFINEMENT_T *x;
  NODE_T *p, *point;
// If there are no refinements, there's little to do.
  if (TOP_REFINEMENT (&(A68 (job))) == NO_REFINEMENT) {
    return;
  }
// Initialisation.
  x = TOP_REFINEMENT (&(A68 (job)));
  while (x != NO_REFINEMENT) {
    APPLICATIONS (x) = 0;
    FORWARD (x);
  }
// Before we introduce infinite loops, find where closing-prelude starts.
  p = TOP_NODE (&(A68 (job)));
  while (p != NO_NODE && IN_PRELUDE (p)) {
    FORWARD (p);
  }
  while (p != NO_NODE && !IN_PRELUDE (p)) {
    FORWARD (p);
  }
  ABEND (p == NO_NODE, ERROR_INTERNAL_CONSISTENCY, __func__);
  point = p;
// We need to substitute until the first point.
  p = TOP_NODE (&(A68 (job)));
  while (p != NO_NODE && ATTRIBUTE (p) != POINT_SYMBOL) {
    if (IS (p, IDENTIFIER)) {
// See if we can find its definition.
      REFINEMENT_T *y = NO_REFINEMENT;
      x = TOP_REFINEMENT (&(A68 (job)));
      while (x != NO_REFINEMENT && y == NO_REFINEMENT) {
        if (NAME (x) == NSYMBOL (p)) {
          y = x;
        } else {
          FORWARD (x);
        }
      }
      if (y != NO_REFINEMENT) {
// We found its definition.
        APPLICATIONS (y)++;
        if (APPLICATIONS (y) > 1) {
          diagnostic (A68_SYNTAX_ERROR, NODE_DEFINED (y), ERROR_REFINEMENT_APPLIED);
          FORWARD (p);
        } else {
// Tie the definition in the tree.
          LINE_APPLIED (y) = LINE (INFO (p));
          if (PREVIOUS (p) != NO_NODE) {
            NEXT (PREVIOUS (p)) = BEGIN (y);
          }
          if (BEGIN (y) != NO_NODE) {
            PREVIOUS (BEGIN (y)) = PREVIOUS (p);
          }
          if (NEXT (p) != NO_NODE) {
            PREVIOUS (NEXT (p)) = END (y);
          }
          if (END (y) != NO_NODE) {
            NEXT (END (y)) = NEXT (p);
          }
          p = BEGIN (y);        // So we can substitute the refinements within
        }
      } else {
        FORWARD (p);
      }
    } else {
      FORWARD (p);
    }
  }
// After the point we ignore it all until the prelude.
  if (p != NO_NODE && IS (p, POINT_SYMBOL)) {
    if (PREVIOUS (p) != NO_NODE) {
      NEXT (PREVIOUS (p)) = point;
    }
    if (PREVIOUS (point) != NO_NODE) {
      PREVIOUS (point) = PREVIOUS (p);
    }
  } else {
    diagnostic (A68_SYNTAX_ERROR, p, ERROR_SYNTAX_EXPECTED, POINT_SYMBOL);
  }
// Has the programmer done it well?.
  if (ERROR_COUNT (&(A68 (job))) == 0) {
    x = TOP_REFINEMENT (&(A68 (job)));
    while (x != NO_REFINEMENT) {
      if (APPLICATIONS (x) == 0) {
        diagnostic (A68_SYNTAX_ERROR, NODE_DEFINED (x), ERROR_REFINEMENT_NOT_APPLIED);
      }
      FORWARD (x);
    }
  }
}
